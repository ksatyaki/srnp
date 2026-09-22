/*
  Pair.cpp - Printing support for Pair.

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

#include <srnp/Pair.h>

#include <format>
#include <ostream>

namespace srnp {
namespace {

std::string_view nameOf(Pair::Type type) {
  switch (type) {
    case Pair::Type::String: return "string";
    case Pair::Type::Bytes: return "bytes";
    case Pair::Type::Meta: return "meta";
    case Pair::Type::Invalid: break;
  }
  return "invalid";
}

/// A default-constructed time point means the field was never set.
std::string describe(TimePoint time) {
  if (time == TimePoint{}) return "unset";
  return std::format("{:%F %T}", std::chrono::floor<std::chrono::milliseconds>(time));
}

}  // namespace

std::string toString(const Pair& pair) {
  return std::format("[{}] {} = \"{}\" ({}, written {})", pair.getOwner(), pair.getKey(),
                     pair.getValue(), nameOf(pair.getType()), describe(pair.getWriteTime()));
}

std::ostream& operator<<(std::ostream& s, const Pair& pair) { return s << toString(pair); }

}  // namespace srnp
