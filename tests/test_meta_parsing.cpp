#include <srnp/client.h>

#include <gtest/gtest.h>

using srnp::extractStrings;

TEST(MetaParsing, SplitsAWellFormedMetaValue) {
  const auto parts = extractStrings("(META 1234 some/key)");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0], "META");
  EXPECT_EQ(parts[1], "1234");
  EXPECT_EQ(parts[2], "some/key");
}

TEST(MetaParsing, HandlesTheNullMetaValue) {
  const auto parts = extractStrings("(META -1 NULL)");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[1], "-1");
  EXPECT_EQ(parts[2], "NULL");
}

TEST(MetaParsing, EmptyInputGivesNoWords) {
  EXPECT_TRUE(extractStrings("").empty());
  EXPECT_TRUE(extractStrings("   ").empty());
  EXPECT_TRUE(extractStrings("()").empty());
}

TEST(MetaParsing, MalformedValuesDoNotYieldThreeWords) {
  // These are what the meta-pair helpers reject, so the word count matters.
  EXPECT_NE(extractStrings("not a meta pair at all").size(), 3u);
  EXPECT_NE(extractStrings("(META 1234)").size(), 3u);
  EXPECT_NE(extractStrings("plain value").size(), 3u);
}

TEST(MetaParsing, ExtraWhitespaceIsIgnored) {
  const auto parts = extractStrings("(META   1234   key)");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[2], "key");
}
