/*
  theme.h - Fonts, icons and the look of the window.

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

#ifndef SRNP_GUI_THEME_H_
#define SRNP_GUI_THEME_H_

struct ImFont;

namespace srnp::gui {

/**
 * Font Awesome 6 Free Solid, as UTF-8. Every one of these is checked to
 * exist in the shipped font; adding a new one means checking it too.
 */
namespace icon {
inline constexpr const char* kSearch = "\xef\x80\x82";      // U+F002 magnifying-glass
inline constexpr const char* kClock = "\xef\x80\x97";       // U+F017 clock
inline constexpr const char* kRotate = "\xef\x80\xa1";      // U+F021 arrows-rotate
inline constexpr const char* kTag = "\xef\x80\xab";         // U+F02B tag
inline constexpr const char* kCircleX = "\xef\x81\x97";     // U+F057 circle-xmark
inline constexpr const char* kCircleCheck = "\xef\x81\x98"; // U+F058 circle-check
inline constexpr const char* kInfo = "\xef\x81\x9a";        // U+F05A circle-info
inline constexpr const char* kArrowRight = "\xef\x81\xa1";  // U+F061 arrow-right
inline constexpr const char* kEye = "\xef\x81\xae";         // U+F06E eye
inline constexpr const char* kWarning = "\xef\x81\xb1";     // U+F071 triangle-exclamation
inline constexpr const char* kFilter = "\xef\x82\xb0";      // U+F0B0 filter
inline constexpr const char* kUsers = "\xef\x83\x80";       // U+F0C0 users
inline constexpr const char* kLink = "\xef\x83\x81";        // U+F0C1 link
inline constexpr const char* kCube = "\xef\x86\xb2";        // U+F1B2 cube
inline constexpr const char* kSend = "\xef\x87\x98";        // U+F1D8 paper-plane
inline constexpr const char* kPlug = "\xef\x87\xa6";        // U+F1E6 plug
inline constexpr const char* kTrash = "\xef\x87\xb8";       // U+F1F8 trash
inline constexpr const char* kServer = "\xef\x88\xb3";      // U+F233 server
}  // namespace icon

/// The fonts, once loaded. Null means the built-in font is in use.
struct Fonts {
  ImFont* body = nullptr;
  ImFont* heading = nullptr;
};

/**
 * Loads IBM Plex Sans with Font Awesome merged into it, so an icon can sit
 * inline in ordinary text. Falls back to ImGui's built-in font, with a note
 * on standard error, if the files are not where they should be.
 */
Fonts loadFonts(float scale);

/// Rounded corners, roomier padding, and a palette that is easier to read
/// for long stretches than the stock dark theme.
void applyStyle(float scale);

/// A help cursor and a tooltip. For controls that are only an icon.
void tooltip(const char* text);

}  // namespace srnp::gui

#endif  // SRNP_GUI_THEME_H_
