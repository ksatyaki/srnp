// End-to-end tests: a real master and real nodes talking over loopback.
// Each node gets its own io_context and pair space, so several can run
// side by side in this one process.

#include <srnp/client.h>
#include <srnp/master_hub.h>
#include <srnp/server.h>
#include <srnp/srnp_print.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using srnp::Pair;

namespace {

constexpr auto kTimeout = 5s;

/// Polls a condition until it holds or the timeout runs out. Used instead
/// of sleeping so the tests stay quick and don't depend on fixed delays.
template <class Predicate>
bool waitFor(Predicate predicate, std::chrono::milliseconds timeout = kTimeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) return true;
    std::this_thread::sleep_for(2ms);
  }
  return predicate();
}

/// The master, running on its own thread.
class TestMaster {
 public:
  TestMaster() {
    // Port 0 lets the OS pick, so tests never collide with a real master.
    hub_ = std::make_unique<srnp::MasterHub>(io_, 0);
    thread_ = std::jthread([this] { io_.run(); });
  }

  ~TestMaster() {
    io_.stop();
    thread_ = {};
  }

  std::string port() const { return std::to_string(hub_->port()); }

 private:
  boost::asio::io_context io_;
  std::unique_ptr<srnp::MasterHub> hub_;
  std::jthread thread_;
};

/// One component: its own server, client and pair space.
class TestNode {
 public:
  TestNode(const std::string& master_port, int desired_owner_id = srnp::kAnyOwner) {
    server_ = std::make_unique<srnp::Server>(io_, "127.0.0.1", master_port, space_, queue_,
                                             desired_owner_id);
    client_ = std::make_unique<srnp::Client>(io_, "127.0.0.1",
                                             std::to_string(server_->getPort()), space_, queue_);
  }

  ~TestNode() {
    // Stop and join the io threads before destroying anything they can
    // still reach, otherwise a resuming coroutine touches a dead Client.
    if (client_) client_->close();
    if (server_) server_->stop();
    client_.reset();
    server_.reset();
  }

  bool waitUntilReady() { return client_->waitUntilReady(kTimeout); }

  srnp::Client& client() { return *client_; }
  int owner() const { return client_->ownerId(); }

 private:
  boost::asio::io_context io_;
  srnp::PairSpace space_;
  srnp::PairQueue queue_;
  std::unique_ptr<srnp::Server> server_;
  std::unique_ptr<srnp::Client> client_;
};

/// Records the values a callback receives.
class Recorder {
 public:
  srnp::Pair::CallbackFunction callback() {
    return [this](const Pair::ConstPtr& pair) {
      std::lock_guard lock(mutex_);
      values_.push_back(pair->getValue());
    };
  }

  std::size_t count() const {
    std::lock_guard lock(mutex_);
    return values_.size();
  }

  std::string last() const {
    std::lock_guard lock(mutex_);
    return values_.empty() ? std::string() : values_.back();
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::string> values_;
};

class Integration : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { srnp::srnp_print_setup("error"); }

  TestMaster master;
};

}  // namespace

TEST_F(Integration, ANodeGetsAnOwnerIdFromTheMaster) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());
  EXPECT_NE(node.owner(), srnp::kAnyOwner);
}

TEST_F(Integration, TwoNodesGetDifferentOwnerIds) {
  TestNode a(master.port());
  TestNode b(master.port());
  ASSERT_TRUE(a.waitUntilReady());
  ASSERT_TRUE(b.waitUntilReady());
  EXPECT_NE(a.owner(), b.owner());
}

TEST_F(Integration, ANodeCanAskForAParticularOwnerId) {
  TestNode node(master.port(), 4242);
  ASSERT_TRUE(node.waitUntilReady());
  EXPECT_EQ(node.owner(), 4242);
}

TEST_F(Integration, APairSetLocallyIsReadableLocally) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  ASSERT_TRUE(node.client().setPair("key", "value"));
  ASSERT_TRUE(waitFor([&] { return node.client().getPair(node.owner(), "key").has_value(); }));
  EXPECT_EQ(node.client().getPair(node.owner(), "key")->getValue(), "value");
}

TEST_F(Integration, GetPairOnAnAbsentKeyIsEmpty) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  // The old isEnd() fell off the end of a non-void function here.
  EXPECT_FALSE(node.client().getPair(node.owner(), "never/set").has_value());
  EXPECT_FALSE(node.client().getPair(9999, "never/set").has_value());
}

TEST_F(Integration, ASubscriberSeesAPairFromAnotherNode) {
  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  // Both nodes need to know about each other before subscribing.
  ASSERT_TRUE(waitFor([&] {
    return subscriber.client().registerSubscription(publisher.owner(), "shared") !=
           srnp::kInvalidSubscriptionHandle;
  }));

  ASSERT_TRUE(waitFor([&] {
    (void)publisher.client().setPair("shared", "hello");
    return subscriber.client().getPair(publisher.owner(), "shared").has_value();
  }));

  EXPECT_EQ(subscriber.client().getPair(publisher.owner(), "shared")->getValue(), "hello");
}

TEST_F(Integration, ACallbackFiresWithTheNewValue) {
  // Declared first so it outlives the nodes: the callback holds &recorder,
  // and a late update can still fire while the node is being torn down.
  Recorder recorder;

  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  subscriber.client().registerSubscription(publisher.owner(), "watched");
  subscriber.client().registerCallback(publisher.owner(), "watched", recorder.callback());

  ASSERT_TRUE(waitFor([&] {
    (void)publisher.client().setPair("watched", "fired");
    return recorder.count() > 0;
  }));
  EXPECT_EQ(recorder.last(), "fired");
}

TEST_F(Integration, ASubscriberJoiningLateStillGetsTheCurrentValue) {
  TestNode publisher(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(publisher.client().setPair("early", "already here"));

  TestNode subscriber(master.port());
  ASSERT_TRUE(subscriber.waitUntilReady());

  ASSERT_TRUE(waitFor([&] {
    subscriber.client().registerSubscription(publisher.owner(), "early");
    return subscriber.client().getPair(publisher.owner(), "early").has_value();
  }));
  EXPECT_EQ(subscriber.client().getPair(publisher.owner(), "early")->getValue(), "already here");
}

TEST_F(Integration, CancellingASubscriptionStopsDelivery) {
  // See ACallbackFiresWithTheNewValue: the recorder must outlive the nodes.
  Recorder recorder;

  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  srnp::SubscriptionHandle handle = srnp::kInvalidSubscriptionHandle;
  ASSERT_TRUE(waitFor([&] {
    handle = subscriber.client().registerSubscription(publisher.owner(), "stream");
    return handle != srnp::kInvalidSubscriptionHandle;
  }));
  subscriber.client().registerCallback(publisher.owner(), "stream", recorder.callback());

  ASSERT_TRUE(waitFor([&] {
    (void)publisher.client().setPair("stream", "before");
    return recorder.count() > 0;
  }));

  subscriber.client().cancelSubscription(handle);
  std::this_thread::sleep_for(200ms);  // Let the cancellation reach the publisher.

  const auto before = recorder.count();
  for (int i = 0; i < 5; ++i) {
    (void)publisher.client().setPair("stream", "after");
    std::this_thread::sleep_for(20ms);
  }
  std::this_thread::sleep_for(200ms);

  EXPECT_EQ(recorder.count(), before);
}

TEST_F(Integration, SubscribingTwiceToTheSamePairIsRefused) {
  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  const auto first = subscriber.client().registerSubscription(publisher.owner(), "once");
  EXPECT_NE(first, srnp::kInvalidSubscriptionHandle);
  EXPECT_EQ(subscriber.client().registerSubscription(publisher.owner(), "once"),
            srnp::kInvalidSubscriptionHandle);
}

TEST_F(Integration, AWildcardKeyDoesNotBlockAPerOwnerSubscription) {
  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  // The old client kept these in one list, so the second call was rejected.
  EXPECT_NE(subscriber.client().registerSubscription("key"), srnp::kInvalidSubscriptionHandle);
  EXPECT_NE(subscriber.client().registerSubscription(publisher.owner(), "key"),
            srnp::kInvalidSubscriptionHandle);
}

TEST_F(Integration, ANodeLeavingDropsItsSubscriptions) {
  TestNode publisher(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());

  int subscriber_owner = srnp::kAnyOwner;
  {
    TestNode subscriber(master.port());
    ASSERT_TRUE(subscriber.waitUntilReady());
    subscriber_owner = subscriber.owner();

    ASSERT_TRUE(waitFor([&] {
      subscriber.client().registerSubscription(publisher.owner(), "shared");
      (void)publisher.client().setPair("shared", "value");
      return subscriber.client().getPair(publisher.owner(), "shared").has_value();
    }));
  }

  // Publishing after the subscriber is gone must not wedge the publisher.
  ASSERT_TRUE(waitFor([&] {
    (void)publisher.client().setPair("shared", "after they left");
    const auto pair = publisher.client().getPair(publisher.owner(), "shared");
    return pair && pair->getValue() == "after they left";
  }));

  EXPECT_NE(subscriber_owner, srnp::kAnyOwner);
}

TEST_F(Integration, SetRemotePairWritesIntoAnotherNode) {
  TestNode writer(master.port());
  TestNode target(master.port());
  ASSERT_TRUE(writer.waitUntilReady());
  ASSERT_TRUE(target.waitUntilReady());

  ASSERT_TRUE(waitFor([&] { return writer.client().setRemotePair(target.owner(), "from", "afar"); }));
  ASSERT_TRUE(
      waitFor([&] { return target.client().getPair(target.owner(), "from").has_value(); }));
  EXPECT_EQ(target.client().getPair(target.owner(), "from")->getValue(), "afar");
}

TEST_F(Integration, ManyPairsInARowAllArrive) {
  TestNode publisher(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());

  for (int i = 0; i < 200; ++i)
    ASSERT_TRUE(publisher.client().setPair("counter", std::to_string(i)));

  // The last write wins, so seeing it means none of the frames desynced.
  ASSERT_TRUE(waitFor([&] {
    const auto pair = publisher.client().getPair(publisher.owner(), "counter");
    return pair && pair->getValue() == "199";
  }));
}

TEST_F(Integration, AMetaPairPointsAtAnotherPair) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  ASSERT_TRUE(node.client().setPair("target", "pointed at"));
  ASSERT_TRUE(node.client().setMetaPair(node.owner(), "pointer", node.owner(), "target"));

  ASSERT_TRUE(waitFor([&] {
    return node.client().getPairIndirectly(node.owner(), "pointer").has_value();
  }));
  EXPECT_EQ(node.client().getPairIndirectly(node.owner(), "pointer")->getValue(), "pointed at");
}

TEST_F(Integration, SettingThroughAMetaPairWritesTheTarget) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  ASSERT_TRUE(node.client().setPair("target", "old"));
  ASSERT_TRUE(node.client().setMetaPair(node.owner(), "pointer", node.owner(), "target"));
  ASSERT_TRUE(waitFor([&] { return node.client().getPair(node.owner(), "pointer").has_value(); }));

  ASSERT_TRUE(node.client().setPairIndirectly(node.owner(), "pointer", "new"));
  ASSERT_TRUE(waitFor([&] {
    const auto pair = node.client().getPair(node.owner(), "target");
    return pair && pair->getValue() == "new";
  }));
}

TEST_F(Integration, AnUnlinkedMetaPairResolvesToNothing) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  ASSERT_TRUE(node.client().initMetaPair(node.owner(), "pointer"));
  ASSERT_TRUE(waitFor([&] { return node.client().getPair(node.owner(), "pointer").has_value(); }));

  EXPECT_FALSE(node.client().getPairIndirectly(node.owner(), "pointer").has_value());
  EXPECT_FALSE(node.client().setPairIndirectly(node.owner(), "pointer", "value"));
}

TEST_F(Integration, ANodeCanDeleteItsOwnPair) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  ASSERT_TRUE(node.client().setPair("doomed", "value"));
  ASSERT_TRUE(waitFor([&] { return node.client().getPair(node.owner(), "doomed").has_value(); }));

  ASSERT_TRUE(node.client().removePair("doomed"));
  EXPECT_TRUE(waitFor([&] { return !node.client().getPair(node.owner(), "doomed").has_value(); }));
}

TEST_F(Integration, ASubscriberSeesAPairDisappear) {
  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  ASSERT_TRUE(waitFor([&] {
    subscriber.client().registerSubscription(publisher.owner(), "shared");
    (void)publisher.client().setPair("shared", "here");
    return subscriber.client().getPair(publisher.owner(), "shared").has_value();
  }));

  ASSERT_TRUE(publisher.client().removePair("shared"));
  EXPECT_TRUE(waitFor([&] {
    return !subscriber.client().getPair(publisher.owner(), "shared").has_value();
  }));
}

TEST_F(Integration, ARePublishAfterADeleteStillReachesTheSubscriber) {
  TestNode publisher(master.port());
  TestNode subscriber(master.port());
  ASSERT_TRUE(publisher.waitUntilReady());
  ASSERT_TRUE(subscriber.waitUntilReady());

  ASSERT_TRUE(waitFor([&] {
    subscriber.client().registerSubscription(publisher.owner(), "recycled");
    (void)publisher.client().setPair("recycled", "first");
    return subscriber.client().getPair(publisher.owner(), "recycled").has_value();
  }));

  ASSERT_TRUE(publisher.client().removePair("recycled"));
  ASSERT_TRUE(waitFor([&] {
    return !subscriber.client().getPair(publisher.owner(), "recycled").has_value();
  }));

  // The subscription outlived the deletion, so no re-subscribe is needed.
  ASSERT_TRUE(publisher.client().setPair("recycled", "second"));
  EXPECT_TRUE(waitFor([&] {
    const auto pair = subscriber.client().getPair(publisher.owner(), "recycled");
    return pair && pair->getValue() == "second";
  }));
}

TEST_F(Integration, DeletingARemotePairDropsItOnTheOwner) {
  TestNode owner(master.port());
  TestNode remover(master.port());
  ASSERT_TRUE(owner.waitUntilReady());
  ASSERT_TRUE(remover.waitUntilReady());

  ASSERT_TRUE(owner.client().setPair("theirs", "value"));
  ASSERT_TRUE(waitFor([&] { return remover.client().removeRemotePair(owner.owner(), "theirs"); }));

  EXPECT_TRUE(waitFor([&] { return !owner.client().getPair(owner.owner(), "theirs").has_value(); }));
}

TEST_F(Integration, DeletingAPairThatIsNotThereIsHarmless) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  ASSERT_TRUE(node.client().removePair("never/set"));

  // The node has to stay usable afterwards.
  ASSERT_TRUE(node.client().setPair("still", "working"));
  EXPECT_TRUE(waitFor([&] { return node.client().getPair(node.owner(), "still").has_value(); }));
}

TEST_F(Integration, ComponentsFollowNodesJoiningAndLeaving) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());
  EXPECT_TRUE(node.client().components().empty());

  int other_owner = srnp::kAnyOwner;
  {
    TestNode other(master.port());
    ASSERT_TRUE(other.waitUntilReady());
    other_owner = other.owner();

    ASSERT_TRUE(waitFor([&] { return node.client().components().contains(other_owner); }));
    const auto info = node.client().components().at(other_owner);
    EXPECT_EQ(info.owner, other_owner);
    EXPECT_FALSE(info.port.empty());
  }

  EXPECT_TRUE(waitFor([&] { return !node.client().components().contains(other_owner); }));
}

TEST_F(Integration, AKeyWithOnlyACallbackOnItReadsAsAbsent) {
  TestNode node(master.port());
  ASSERT_TRUE(node.waitUntilReady());

  // Registering leaves a placeholder in the pair space. It is bookkeeping,
  // not a pair, so it must not read back as one.
  node.client().registerCallback(node.owner(), "watched", [](const Pair::ConstPtr&) {});
  EXPECT_FALSE(node.client().getPair(node.owner(), "watched").has_value());
}
