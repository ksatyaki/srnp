#include "../gui/format.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace std::chrono_literals;
using namespace srnp::gui;
using srnp::Pair;

TEST(Format, EscapeLeavesPrintableTextAlone) {
  EXPECT_EQ(escape("hello world"), "hello world");
  EXPECT_EQ(escape(""), "");
}

TEST(Format, EscapeHandlesEmbeddedNullsAndHighBytes) {
  EXPECT_EQ(escape(std::string("a\0b", 3)), "a\\x00b");
  EXPECT_EQ(escape("\xff\x80"), "\\xff\\x80");
  EXPECT_EQ(escape("\x1b["), "\\x1b[");
}

TEST(Format, EscapeSpellsOutTheCommonControlCharacters) {
  EXPECT_EQ(escape("a\nb\tc\rd"), "a\\nb\\tc\\rd");
  EXPECT_EQ(escape("back\\slash"), "back\\\\slash");
}

TEST(Format, EscapeTruncatedCutsTheEscapedText) {
  EXPECT_EQ(escapeTruncated("hello", 10), "hello");
  EXPECT_EQ(escapeTruncated("hello world", 5), "hello...");

  // An escape counts by the characters shown, not the bytes behind them.
  EXPECT_EQ(escapeTruncated(std::string("\0\0\0", 3), 4), "\\x00...");
}

TEST(Format, HexDumpOfAShortValue) {
  EXPECT_EQ(hexDump("Hi"),
            "00000000  48 69                                             |Hi|\n");
}

TEST(Format, HexDumpWrapsAtSixteenBytes) {
  const auto dump = hexDump(std::string(20, 'A'));
  EXPECT_EQ(std::count(dump.begin(), dump.end(), '\n'), 2);
  EXPECT_TRUE(dump.starts_with("00000000  41 41"));
  EXPECT_NE(dump.find("00000010  41 41 41 41"), std::string::npos);
}

TEST(Format, HexDumpShowsUnprintableBytesAsDots) {
  EXPECT_NE(hexDump(std::string("a\0b", 3)).find("|a.b|"), std::string::npos);
}

TEST(Format, HexDumpOfNothingIsNothing) {
  EXPECT_EQ(hexDump(""), "");
}

TEST(Format, FormatAge) {
  EXPECT_EQ(formatAge(0s), "just now");
  EXPECT_EQ(formatAge(900ms), "just now");
  EXPECT_EQ(formatAge(4s), "4 s ago");
  EXPECT_EQ(formatAge(130s), "2 m 10 s ago");
  EXPECT_EQ(formatAge(3900s), "1 h 5 m ago");
  EXPECT_EQ(formatAge(90000s), "1 d 1 h ago");
}

TEST(Format, AnAgeFromAClockAheadOfOursReadsAsSuch) {
  EXPECT_EQ(formatAge(-5s), "in the future");
}

TEST(Format, FormatTime) {
  EXPECT_EQ(formatTime(srnp::TimePoint{}), "never");
  EXPECT_NE(formatTime(srnp::Clock::now()), "never");
}

TEST(Format, ParseMetaTarget) {
  const auto target = parseMetaTarget("(META 1000 temperature)");
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->owner, 1000);
  EXPECT_EQ(target->key, "temperature");
}

TEST(Format, ParseMetaTargetOfAnUnlinkedMetaPair) {
  const auto target = parseMetaTarget("(META -1 NULL)");
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->owner, -1);
  EXPECT_EQ(target->key, "NULL");
}

TEST(Format, ParseMetaTargetRejectsAnythingElse) {
  EXPECT_FALSE(parseMetaTarget("just a value").has_value());
  EXPECT_FALSE(parseMetaTarget("").has_value());
  EXPECT_FALSE(parseMetaTarget("(META notanumber key)").has_value());
  EXPECT_FALSE(parseMetaTarget("(META 1000)").has_value());
  EXPECT_FALSE(parseMetaTarget("(NOTMETA 1000 key)").has_value());
}

TEST(Format, MatchesFilter) {
  EXPECT_TRUE(matchesFilter("temperature", ""));
  EXPECT_TRUE(matchesFilter("temperature", "temp"));
  EXPECT_TRUE(matchesFilter("temperature", "TEMP"));
  EXPECT_TRUE(matchesFilter("TEMPERATURE", "rat"));
  EXPECT_FALSE(matchesFilter("temperature", "humidity"));
  EXPECT_FALSE(matchesFilter("temp", "temperature"));
}

TEST(Format, NameOfType) {
  EXPECT_EQ(nameOfType(Pair::Type::String), "String");
  EXPECT_EQ(nameOfType(Pair::Type::Bytes), "Bytes");
  EXPECT_EQ(nameOfType(Pair::Type::Meta), "Meta");
  EXPECT_EQ(nameOfType(Pair::Type::Invalid), "Invalid");
}

TEST(Format, BriefErrorDropsBoostsDiagnostics) {
  EXPECT_EQ(briefError("could not reach the master at 127.0.0.1:12399 (connect: "
                       "Connection refused [system:111 at /usr/include/boost/asio/"
                       "detail/reactive_socket_service.hpp:587:5 in function 'x'])"),
            "could not reach the master at 127.0.0.1:12399 (connect: Connection refused)");
}

TEST(Format, BriefErrorLeavesAPlainMessageAlone) {
  EXPECT_EQ(briefError("the master never sent us an owner id"),
            "the master never sent us an owner id");
  EXPECT_EQ(briefError(""), "");
}
