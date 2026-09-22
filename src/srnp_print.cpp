/*
  srnp_print.cpp - Implementation of the logger.

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

#include <srnp/srnp_print.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>

namespace srnp {
namespace {

std::atomic<LogLevel> g_level{LogLevel::Info};

// Serialises writes so lines from different threads don't interleave.
std::mutex g_write_mutex;

constexpr std::array<std::pair<std::string_view, LogLevel>, 7> kLevelNames{{
    {"trace", LogLevel::Trace},
    {"debug", LogLevel::Debug},
    {"info", LogLevel::Info},
    {"warning", LogLevel::Warning},
    {"error", LogLevel::Error},
    {"fatal", LogLevel::Fatal},
    {"off", LogLevel::Off},
}};

std::string_view nameOf(LogLevel level) {
  for (const auto& [name, value] : kLevelNames)
    if (value == level) return name;
  return "?";
}

}  // namespace

bool srnp_print_setup(std::string_view level) {
  std::string lowered(level);
  std::ranges::transform(lowered, lowered.begin(),
                         [](unsigned char c) { return std::tolower(c); });

  for (const auto& [name, value] : kLevelNames) {
    if (name == lowered) {
      g_level.store(value, std::memory_order_relaxed);
      return true;
    }
  }
  return false;
}

LogLevel logLevel() { return g_level.load(std::memory_order_relaxed); }

void logLine(LogLevel level, std::string_view message, const std::source_location& where) {
  const auto now = std::chrono::floor<std::chrono::milliseconds>(
      std::chrono::system_clock::now());

  std::string line = std::format("[{:%H:%M:%S}] ({}): {}", now, nameOf(level), message);

  // Only the levels you'd actually go looking for get a file and line.
  if (level >= LogLevel::Warning) {
    line += std::format(" [{}:{}]",
                        std::filesystem::path(where.file_name()).filename().string(),
                        where.line());
  }
  line += '\n';

  std::FILE* stream = level >= LogLevel::Warning ? stderr : stdout;

  std::lock_guard lock(g_write_mutex);
  std::fputs(line.c_str(), stream);
  // Flush every line: a log that loses its last entries when the process is
  // killed is no use for working out why it was killed.
  std::fflush(stream);
}

}  // namespace srnp
