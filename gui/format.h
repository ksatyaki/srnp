/*
  format.h - Turning pair data into text. No ImGui in here, so it can be
  unit tested on its own.

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

#ifndef SRNP_GUI_FORMAT_H_
#define SRNP_GUI_FORMAT_H_

#include <srnp/Pair.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace srnp::gui {

/**
 * Makes a value safe to display. A pair value is a byte string: it can hold
 * nulls, control characters and anything else, none of which can be handed
 * to a text widget as it stands.
 */
std::string escape(std::string_view value);

/// The same, cut to at most max_chars with a trailing ellipsis.
std::string escapeTruncated(std::string_view value, std::size_t max_chars);

/// Offset, hex, then the printable column. Sixteen bytes per line.
std::string hexDump(std::string_view bytes);

/// "just now", "4 s ago", "2 m 10 s ago", "1 h 5 m ago". A negative age
/// reads "in the future"; clocks on two hosts need not agree.
std::string formatAge(Clock::duration age);

/// Absolute local time, to the second. "never" for a default-constructed
/// time point, which is what an unset expiry looks like.
std::string formatTime(TimePoint time);

/// What a meta-pair points at.
struct MetaTarget {
  int owner = kAnyOwner;
  std::string key;
};

/// Reads "(META 1000 temperature)", or nullopt if it is not one.
std::optional<MetaTarget> parseMetaTarget(std::string_view value);

/// Cuts an exception message down to its first sentence. Boost.Asio's
/// messages carry a header path and a full function signature after a " [",
/// which is of no use to someone reading a banner.
std::string briefError(std::string_view what);

/// Case-insensitive substring match. An empty filter matches everything.
bool matchesFilter(std::string_view text, std::string_view filter);

/// "String", "Bytes", "Meta", "Invalid".
std::string_view nameOfType(Pair::Type type);

}  // namespace srnp::gui

#endif  // SRNP_GUI_FORMAT_H_
