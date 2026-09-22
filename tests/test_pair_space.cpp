#include <srnp/PairSpace.h>

#include <gtest/gtest.h>

using srnp::Pair;
using srnp::PairEntry;
using srnp::PairSpace;

namespace {

Pair makePair(int owner, std::string key, std::string value) {
  return Pair(owner, std::move(key), std::move(value), Pair::Type::String);
}

}  // namespace

TEST(PairSpace, FindReturnsNullForAnAbsentPair) {
  PairSpace space;
  EXPECT_EQ(space.find(1, "nothing"), nullptr);
  EXPECT_FALSE(space.copyOf(1, "nothing").has_value());
}

TEST(PairSpace, AddThenFind) {
  PairSpace space;
  space.addPair(makePair(1, "key", "value"));

  const PairEntry* found = space.find(1, "key");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->pair.getValue(), "value");
  EXPECT_NE(found->pair.getWriteTime(), srnp::TimePoint{});
}

TEST(PairSpace, OwnerIsPartOfTheIdentity) {
  PairSpace space;
  space.addPair(makePair(1, "key", "from one"));
  space.addPair(makePair(2, "key", "from two"));

  EXPECT_EQ(space.find(1, "key")->pair.getValue(), "from one");
  EXPECT_EQ(space.find(2, "key")->pair.getValue(), "from two");
  EXPECT_EQ(space.getAllPairs().size(), 2u);
}

TEST(PairSpace, UpdateKeepsSubscribersAndCallbacks) {
  PairSpace space;
  space.addPair(makePair(1, "key", "first"));
  space.addSubscription(1, "key", 42);
  space.addCallback(1, "key", [](const Pair::ConstPtr&) {});

  space.addPair(makePair(1, "key", "second"));

  const PairEntry* found = space.find(1, "key");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->pair.getValue(), "second");
  EXPECT_EQ(found->subscribers, std::vector<int>{42});
  EXPECT_EQ(found->callbacks.size(), 1u);
}

TEST(PairSpace, RemovePair) {
  PairSpace space;
  space.addPair(makePair(1, "key", "value"));
  space.removePair(1, "key");
  EXPECT_EQ(space.find(1, "key"), nullptr);

  // Removing something that isn't there is not an error.
  space.removePair(1, "key");
}

TEST(PairSpace, RemovePairKeepsSubscribersAndCallbacks) {
  PairSpace space;
  space.addPair(makePair(1, "key", "value"));
  space.addSubscription(1, "key", 42);
  space.addCallback(1, "key", [](const Pair::ConstPtr&) {});

  space.removePair(1, "key");

  // What is left is the placeholder a subscription to an unpublished key
  // would have created, so re-publishing still reaches subscriber 42.
  const PairEntry* placeholder = space.find(1, "key");
  ASSERT_NE(placeholder, nullptr);
  EXPECT_EQ(placeholder->pair.getType(), Pair::Type::Invalid);
  EXPECT_EQ(placeholder->pair.getValue(), "");
  EXPECT_EQ(placeholder->subscribers, std::vector<int>{42});
  EXPECT_EQ(placeholder->callbacks.size(), 1u);

  space.addPair(makePair(1, "key", "again"));
  EXPECT_EQ(space.find(1, "key")->subscribers, std::vector<int>{42});
}

TEST(PairSpace, RemovePairDropsTheEntryOnceNobodyIsListening) {
  PairSpace space;
  space.addPair(makePair(1, "key", "value"));
  space.addSubscription(1, "key", 42);

  space.removePair(1, "key");
  space.removeSubscription(1, "key", 42);
  space.removePair(1, "key");

  EXPECT_EQ(space.find(1, "key"), nullptr);
}

TEST(PairSpace, SubscriptionBeforeThePairExists) {
  PairSpace space;
  space.addSubscription(7, "later", 42);

  const PairEntry* placeholder = space.find(7, "later");
  ASSERT_NE(placeholder, nullptr);
  EXPECT_EQ(placeholder->pair.getType(), Pair::Type::Invalid);
  EXPECT_EQ(placeholder->subscribers, std::vector<int>{42});

  space.addPair(makePair(7, "later", "now it exists"));
  EXPECT_EQ(space.find(7, "later")->subscribers, std::vector<int>{42});
}

TEST(PairSpace, DuplicateSubscriptionIsIgnored) {
  PairSpace space;
  space.addSubscription(1, "key", 42);
  space.addSubscription(1, "key", 42);
  EXPECT_EQ(space.find(1, "key")->subscribers.size(), 1u);
}

TEST(PairSpace, RemoveSubscription) {
  PairSpace space;
  space.addSubscription(1, "key", 42);
  space.removeSubscription(1, "key", 42);
  EXPECT_TRUE(space.find(1, "key")->subscribers.empty());
}

TEST(PairSpace, SubscribeToAllCoversPairsAddedLater) {
  PairSpace space;
  space.addPair(makePair(1, "before", "x"));
  space.addSubscriptionToAll(42);
  space.addPair(makePair(1, "after", "y"));

  EXPECT_EQ(space.find(1, "before")->subscribers, std::vector<int>{42});
  EXPECT_EQ(space.find(1, "after")->subscribers, std::vector<int>{42});
}

TEST(PairSpace, RemoveSubscriptionToAllClearsEveryPair) {
  PairSpace space;
  space.addPair(makePair(1, "a", "x"));
  space.addSubscriptionToAll(42);
  space.addSubscription(1, "b", 42);

  space.removeSubscriptionToAll(42);

  EXPECT_TRUE(space.find(1, "a")->subscribers.empty());
  EXPECT_TRUE(space.find(1, "b")->subscribers.empty());

  // A pair added afterwards must not inherit the cancelled subscription.
  space.addPair(makePair(1, "c", "z"));
  EXPECT_TRUE(space.find(1, "c")->subscribers.empty());
}

TEST(PairSpace, CallbackHandlesAreUniqueAndNeverZero) {
  PairSpace space;
  std::set<srnp::CallbackHandle> handles;

  for (int i = 0; i < 5000; ++i) {
    const auto handle = space.addCallback(1, "key", [](const Pair::ConstPtr&) {});
    EXPECT_NE(handle, srnp::kInvalidCallbackHandle);
    EXPECT_TRUE(handles.insert(handle).second) << "handle " << handle << " was reused";
  }
  EXPECT_EQ(space.find(1, "key")->callbacks.size(), 5000u);
}

TEST(PairSpace, RemoveCallback) {
  PairSpace space;
  const auto handle = space.addCallback(1, "key", [](const Pair::ConstPtr&) {});
  space.removeCallback(handle);
  EXPECT_TRUE(space.find(1, "key")->callbacks.empty());

  // Removing a handle twice, or one that never existed, must not crash.
  space.removeCallback(handle);
  space.removeCallback(999999);
}

TEST(PairSpace, RemoveCallbackAfterItsPairIsGone) {
  PairSpace space;
  const auto handle = space.addCallback(1, "key", [](const Pair::ConstPtr&) {});
  space.removePair(1, "key");
  space.removeCallback(handle);  // Used to dereference a past-the-end iterator.
}
