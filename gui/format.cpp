/*
  format.cpp - Implementation of the display helpers.

  Copyright (C) 2015  Chittaranjan Srinivas Swaminathan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "format.h"

#include <srnp/client.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <format>

namespace srnp::gui {
namespace {

constexpr std::size_t kBytesPerLine = 16;

bool isPrintable(unsigned char byte) { return byte >= 0x20 && byte < 0x7F; }

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

}  // namespace

std::string escape(std::string_view value) {
  std::string out;
  out.reserve(value.size());

  for (const char c : value) {
    const auto byte = static_cast<unsigned char>(c);
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (isPrintable(byte))
          out += c;
        else
          out += std::format("\\x{:02x}", byte);
    }
  }
  return out;
}

std::string escapeTruncated(std::string_view value, std::size_t max_chars) {
  auto text = escape(value);
  if (text.size() <= max_chars) return text;

  text.resize(max_chars);
  text += "...";
  return text;
}

std::string hexDump(std::string_view bytes) {
  std::string out;

  for (std::size_t start = 0; start < bytes.size(); start += kBytesPerLine) {
    const auto line = bytes.substr(start, kBytesPerLine);

    out += std::format("{:08x}  ", start);
    for (std::size_t i = 0; i < kBytesPerLine; ++i) {
      // Short last line still pads, so the printable column stays aligned.
      if (i < line.size())
        out += std::format("{:02x} ", static_cast<unsigned char>(line[i]));
      else
        out += "   ";
      if (i == kBytesPerLine / 2 - 1) out += ' ';
    }

    out += " |";
    for (const char c : line) out += isPrintable(static_cast<unsigned char>(c)) ? c : '.';
    out += "|\n";
  }
  return out;
}

std::string formatAge(Clock::duration age) {
  using namespace std::chrono;

  if (age < 0s) return "in the future";
  if (age < 1s) return "just now";

  const auto total = duration_cast<seconds>(age).count();
  if (total < 60) return std::format("{} s ago", total);
  if (total < 3600) return std::format("{} m {} s ago", total / 60, total % 60);
  if (total < 86400) return std::format("{} h {} m ago", total / 3600, (total % 3600) / 60);
  return std::format("{} d {} h ago", total / 86400, (total % 86400) / 3600);
}

std::string formatTime(TimePoint time) {
  if (time == TimePoint{}) return "never";
  return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(time));
}

std::optional<MetaTarget> parseMetaTarget(std::string_view value) {
  // The same parser the library uses, so the two cannot disagree on the format.
  const auto parts = extractStrings(value);
  if (parts.size() != 3 || parts[0] != "META") return std::nullopt;

  MetaTarget target;
  const auto* end = parts[1].data() + parts[1].size();
  if (std::from_chars(parts[1].data(), end, target.owner).ptr != end) return std::nullopt;

  target.key = parts[2];
  return target;
}

std::string briefError(std::string_view what) {
  const auto detail = what.find(" [");
  if (detail == std::string_view::npos) return std::string(what);

  auto brief = std::string(what.substr(0, detail));
  // The cut can leave brackets opened earlier unclosed.
  const auto opened = std::ranges::count(brief, '(');
  const auto closed = std::ranges::count(brief, ')');
  brief.append(static_cast<std::size_t>(opened - closed), ')');
  return brief;
}

bool matchesFilter(std::string_view text, std::string_view filter) {
  if (filter.empty()) return true;
  if (filter.size() > text.size()) return false;

  const auto equal = [](char a, char b) { return lower(a) == lower(b); };
  return std::ranges::search(text, filter, equal).begin() != text.end();
}

std::string_view nameOfType(Pair::Type type) {
  switch (type) {
    case Pair::Type::String: return "String";
    case Pair::Type::Bytes: return "Bytes";
    case Pair::Type::Meta: return "Meta";
    case Pair::Type::Invalid: break;
  }
  return "Invalid";
}

}  // namespace srnp::gui
