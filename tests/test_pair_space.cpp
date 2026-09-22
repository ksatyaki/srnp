#include <srnp/PairSpace.h>

#include <gtest/gtest.h>

using srnp::Pair;
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

  const Pair* found = space.find(1, "key");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getValue(), "value");
  EXPECT_NE(found->getWriteTime(), srnp::TimePoint{});
}

TEST(PairSpace, OwnerIsPartOfTheIdentity) {
  PairSpace space;
  space.addPair(makePair(1, "key", "from one"));
  space.addPair(makePair(2, "key", "from two"));

  EXPECT_EQ(space.find(1, "key")->getValue(), "from one");
  EXPECT_EQ(space.find(2, "key")->getValue(), "from two");
  EXPECT_EQ(space.getAllPairs().size(), 2u);
}

TEST(PairSpace, UpdateKeepsSubscribersAndCallbacks) {
  PairSpace space;
  space.addPair(makePair(1, "key", "first"));
  space.addSubscription(1, "key", 42);
  space.addCallback(1, "key", [](const Pair::ConstPtr&) {});

  space.addPair(makePair(1, "key", "second"));

  const Pair* found = space.find(1, "key");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getValue(), "second");
  EXPECT_EQ(found->subscribers_, std::vector<int>{42});
  EXPECT_EQ(found->callbacks_.size(), 1u);
}

TEST(PairSpace, RemovePair) {
  PairSpace space;
  space.addPair(makePair(1, "key", "value"));
  space.removePair(1, "key");
  EXPECT_EQ(space.find(1, "key"), nullptr);

  // Removing something that isn't there is not an error.
  space.removePair(1, "key");
}

TEST(PairSpace, SubscriptionBeforeThePairExists) {
  PairSpace space;
  space.addSubscription(7, "later", 42);

  const Pair* placeholder = space.find(7, "later");
  ASSERT_NE(placeholder, nullptr);
  EXPECT_EQ(placeholder->getType(), Pair::Type::Invalid);
  EXPECT_EQ(placeholder->subscribers_, std::vector<int>{42});

  space.addPair(makePair(7, "later", "now it exists"));
  EXPECT_EQ(space.find(7, "later")->subscribers_, std::vector<int>{42});
}

TEST(PairSpace, DuplicateSubscriptionIsIgnored) {
  PairSpace space;
  space.addSubscription(1, "key", 42);
  space.addSubscription(1, "key", 42);
  EXPECT_EQ(space.find(1, "key")->subscribers_.size(), 1u);
}

TEST(PairSpace, RemoveSubscription) {
  PairSpace space;
  space.addSubscription(1, "key", 42);
  space.removeSubscription(1, "key", 42);
  EXPECT_TRUE(space.find(1, "key")->subscribers_.empty());
}

TEST(PairSpace, SubscribeToAllCoversPairsAddedLater) {
  PairSpace space;
  space.addPair(makePair(1, "before", "x"));
  space.addSubscriptionToAll(42);
  space.addPair(makePair(1, "after", "y"));

  EXPECT_EQ(space.find(1, "before")->subscribers_, std::vector<int>{42});
  EXPECT_EQ(space.find(1, "after")->subscribers_, std::vector<int>{42});
}

TEST(PairSpace, RemoveSubscriptionToAllClearsEveryPair) {
  PairSpace space;
  space.addPair(makePair(1, "a", "x"));
  space.addSubscriptionToAll(42);
  space.addSubscription(1, "b", 42);

  space.removeSubscriptionToAll(42);

  EXPECT_TRUE(space.find(1, "a")->subscribers_.empty());
  EXPECT_TRUE(space.find(1, "b")->subscribers_.empty());

  // A pair added afterwards must not inherit the cancelled subscription.
  space.addPair(makePair(1, "c", "z"));
  EXPECT_TRUE(space.find(1, "c")->subscribers_.empty());
}

TEST(PairSpace, CallbackHandlesAreUniqueAndNeverZero) {
  PairSpace space;
  std::set<srnp::CallbackHandle> handles;

  for (int i = 0; i < 5000; ++i) {
    const auto handle = space.addCallback(1, "key", [](const Pair::ConstPtr&) {});
    EXPECT_NE(handle, srnp::kInvalidCallbackHandle);
    EXPECT_TRUE(handles.insert(handle).second) << "handle " << handle << " was reused";
  }
  EXPECT_EQ(space.find(1, "key")->callbacks_.size(), 5000u);
}

TEST(PairSpace, RemoveCallback) {
  PairSpace space;
  const auto handle = space.addCallback(1, "key", [](const Pair::ConstPtr&) {});
  space.removeCallback(handle);
  EXPECT_TRUE(space.find(1, "key")->callbacks_.empty());

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
