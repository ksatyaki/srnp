/*
  theme.cpp - Implementation of the fonts and the style.

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

#include "theme.h"

#include <imgui.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace srnp::gui {
namespace {

constexpr float kBodySize = 21.0f;
constexpr float kHeadingSize = 24.0f;
/// Icons sit slightly below the text baseline at their natural size.
constexpr float kIconSize = 18.0f;

/// Where the fonts are looked for, installed location first.
const char* const kFontDirs[] = {SRNP_FONT_DIR_INSTALLED, SRNP_FONT_DIR_SOURCE};

/// Only the icons actually used are rasterised, not the whole font.
constexpr ImWchar kIconRange[] = {0xf000, 0xf2ff, 0};

std::filesystem::path findFontDir() {
  for (const char* dir : kFontDirs) {
    const std::filesystem::path candidate(dir);
    std::error_code ignored;
    if (std::filesystem::exists(candidate / "IBMPlexSans-Regular.ttf", ignored))
      return candidate;
  }
  return {};
}

/// Loads one face and merges the icons into it, so icons can be written
/// inline in any string without switching fonts.
ImFont* loadFace(const std::filesystem::path& dir, const char* file, float size,
                 float icon_size) {
  ImGuiIO& io = ImGui::GetIO();

  ImFont* font = io.Fonts->AddFontFromFileTTF((dir / file).c_str(), size);
  if (font == nullptr) return nullptr;

  ImFontConfig config;
  config.MergeMode = true;
  // Nudged down so the icons line up with the middle of the text.
  config.GlyphOffset = ImVec2(0.0f, 2.0f);
  config.GlyphMinAdvanceX = icon_size;
  io.Fonts->AddFontFromFileTTF((dir / "fa-solid-900.ttf").c_str(), icon_size, &config,
                               kIconRange);
  return font;
}

ImVec4 rgb(int r, int g, int b, float alpha = 1.0f) {
  return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f, alpha);
}

}  // namespace

Fonts loadFonts(float scale) {
  const auto dir = findFontDir();
  if (dir.empty()) {
    std::fprintf(stderr,
                 "could not find the shipped fonts in %s or %s; "
                 "falling back to the built-in one\n",
                 SRNP_FONT_DIR_INSTALLED, SRNP_FONT_DIR_SOURCE);
    return {};
  }

  Fonts fonts;
  fonts.body = loadFace(dir, "IBMPlexSans-Regular.ttf", kBodySize * scale, kIconSize * scale);
  fonts.heading =
      loadFace(dir, "IBMPlexSans-SemiBold.ttf", kHeadingSize * scale, kIconSize * scale);

  if (fonts.body == nullptr) {
    std::fprintf(stderr, "the fonts in %s could not be read; falling back\n", dir.c_str());
    return {};
  }
  return fonts;
}

void applyStyle(float scale) {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();

  // Everything with a corner gets the same radius, so no control looks like
  // it came from somewhere else.
  style.WindowRounding = 10.0f;
  style.ChildRounding = 10.0f;
  style.PopupRounding = 10.0f;
  style.FrameRounding = 8.0f;
  style.GrabRounding = 8.0f;
  style.ScrollbarRounding = 10.0f;
  style.TabRounding = 8.0f;

  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;

  style.WindowPadding = ImVec2(16, 14);
  style.FramePadding = ImVec2(12, 7);
  style.CellPadding = ImVec2(10, 6);
  style.ItemSpacing = ImVec2(10, 9);
  style.ItemInnerSpacing = ImVec2(8, 6);
  style.IndentSpacing = 22.0f;
  style.ScrollbarSize = 13.0f;
  style.GrabMinSize = 12.0f;
  style.SeparatorTextBorderSize = 2.0f;
  style.SeparatorTextPadding = ImVec2(20, 6);

  ImVec4* colours = style.Colors;
  colours[ImGuiCol_WindowBg] = rgb(24, 26, 31);
  colours[ImGuiCol_ChildBg] = rgb(29, 32, 38);
  colours[ImGuiCol_PopupBg] = rgb(32, 35, 42);
  colours[ImGuiCol_Border] = rgb(52, 57, 67);

  colours[ImGuiCol_FrameBg] = rgb(38, 42, 50);
  colours[ImGuiCol_FrameBgHovered] = rgb(48, 53, 63);
  colours[ImGuiCol_FrameBgActive] = rgb(56, 62, 74);

  colours[ImGuiCol_Button] = rgb(52, 90, 140);
  colours[ImGuiCol_ButtonHovered] = rgb(64, 110, 170);
  colours[ImGuiCol_ButtonActive] = rgb(44, 78, 122);

  colours[ImGuiCol_Header] = rgb(52, 90, 140, 0.65f);
  colours[ImGuiCol_HeaderHovered] = rgb(64, 110, 170, 0.75f);
  colours[ImGuiCol_HeaderActive] = rgb(64, 110, 170);

  colours[ImGuiCol_CheckMark] = rgb(120, 190, 255);
  colours[ImGuiCol_SliderGrab] = rgb(64, 110, 170);
  colours[ImGuiCol_Separator] = rgb(52, 57, 67);
  colours[ImGuiCol_TableHeaderBg] = rgb(38, 42, 50);
  colours[ImGuiCol_TableBorderLight] = rgb(44, 48, 57);
  colours[ImGuiCol_TableRowBgAlt] = rgb(31, 34, 41);

  colours[ImGuiCol_Text] = rgb(226, 230, 238);
  colours[ImGuiCol_TextDisabled] = rgb(126, 134, 148);

  style.ScaleAllSizes(scale);
}

void tooltip(const char* text) {
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) return;
  ImGui::SetTooltip("%s", text);
}

}  // namespace srnp::gui
