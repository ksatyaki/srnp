/*
  srnp_print.h - Logging.

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

#ifndef SRNP_PRINT_H
#define SRNP_PRINT_H

#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace srnp {

enum class LogLevel { Trace, Debug, Info, Warning, Error, Fatal, Off };

/**
 * Set the minimum level that gets printed. Accepts the level names above,
 * in any case. An unknown name leaves the level unchanged and returns false.
 */
bool srnp_print_setup(std::string_view level);

LogLevel logLevel();

/**
 * Print one already-formatted line. Messages below the current level are
 * dropped. Warnings and worse carry the source location.
 */
void logLine(LogLevel level, std::string_view message, const std::source_location& where);

/**
 * Formats and prints, but only if the level passes. Taking the format
 * arguments lazily keeps disabled log statements close to free.
 */
template <class... Args>
void logAt(LogLevel level, const std::source_location& where,
           std::format_string<Args...> fmt, Args&&... args) {
  if (level < logLevel()) return;
  logLine(level, std::format(fmt, std::forward<Args>(args)...), where);
}

}  // namespace srnp

#define SRNP_TRACE(...) \
  ::srnp::logAt(::srnp::LogLevel::Trace, std::source_location::current(), __VA_ARGS__)
#define SRNP_DEBUG(...) \
  ::srnp::logAt(::srnp::LogLevel::Debug, std::source_location::current(), __VA_ARGS__)
#define SRNP_INFO(...) \
  ::srnp::logAt(::srnp::LogLevel::Info, std::source_location::current(), __VA_ARGS__)
#define SRNP_WARN(...) \
  ::srnp::logAt(::srnp::LogLevel::Warning, std::source_location::current(), __VA_ARGS__)
#define SRNP_ERROR(...) \
  ::srnp::logAt(::srnp::LogLevel::Error, std::source_location::current(), __VA_ARGS__)
#define SRNP_FATAL(...) \
  ::srnp::logAt(::srnp::LogLevel::Fatal, std::source_location::current(), __VA_ARGS__)

#endif  // SRNP_PRINT_H
