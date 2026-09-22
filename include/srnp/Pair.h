/*
  Pair.h - Defines the basic type used in SRNP - Pair.

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

#ifndef PAIR_H_
#define PAIR_H_

#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace srnp {

using CallbackHandle = std::uint64_t;
using SubscriptionHandle = std::uint64_t;

/// Returned when a registration fails. Never a valid handle.
inline constexpr CallbackHandle kInvalidCallbackHandle = 0;
inline constexpr SubscriptionHandle kInvalidSubscriptionHandle = 0;

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

/// Stands in for "any owner" in a wildcard subscription.
inline constexpr int kAnyOwner = -1;

class Pair {
 public:
  enum class Type : std::uint8_t { Invalid = 0, String, Bytes, Meta };

  using Ptr = std::shared_ptr<Pair>;
  using ConstPtr = std::shared_ptr<const Pair>;
  using CallbackFunction = std::function<void(const ConstPtr&)>;

  Pair() = default;

  Pair(int owner, std::string key, std::string value, Type type = Type::String)
      : pair_(std::move(key), std::move(value)), owner_(owner), pair_type_(type) {}

  void setPair(std::string key, std::string value) {
    pair_.first = std::move(key);
    pair_.second = std::move(value);
  }

  void setValue(std::string value) { pair_.second = std::move(value); }
  void setType(Type type) { pair_type_ = type; }
  void setOwner(int owner) { owner_ = owner; }
  void setWriteTime(TimePoint write_time) { write_time_ = write_time; }
  void setExpiryTime(TimePoint expiry_time) { expiry_time_ = expiry_time; }

  int getOwner() const { return owner_; }
  const std::string& getKey() const { return pair_.first; }
  const std::string& getValue() const { return pair_.second; }
  Type getType() const { return pair_type_; }
  const std::pair<std::string, std::string>& getPair() const { return pair_; }
  TimePoint getWriteTime() const { return write_time_; }
  TimePoint getExpiryTime() const { return expiry_time_; }

 private:
  std::pair<std::string, std::string> pair_;
  int owner_ = kAnyOwner;
  Pair::Type pair_type_ = Type::Invalid;
  TimePoint write_time_{};
  TimePoint expiry_time_{};
};

using PairPtr = Pair::Ptr;

/// Identifies a pair anywhere in the system.
struct PairKey {
  int owner = kAnyOwner;
  std::string key;

  auto operator<=>(const PairKey&) const = default;
};

/// A PairKey that borrows its string, for lookups that shouldn't allocate.
struct PairKeyView {
  int owner = kAnyOwner;
  std::string_view key;
};

/// Lets a map keyed by PairKey be searched with a PairKeyView.
struct PairKeyLess {
  using is_transparent = void;

  static auto asView(const PairKey& k) { return PairKeyView{k.owner, k.key}; }
  static auto asView(const PairKeyView& k) { return k; }

  template <class A, class B>
  bool operator()(const A& a, const B& b) const {
    const auto left = asView(a);
    const auto right = asView(b);
    if (left.owner != right.owner) return left.owner < right.owner;
    return left.key < right.key;
  }
};

std::ostream& operator<<(std::ostream& s, const Pair& pair);

/// The same text a Pair streams out, as a string.
std::string toString(const Pair& pair);

}  // namespace srnp

/// Lets a Pair be passed straight to the logging macros.
template <>
struct std::formatter<srnp::Pair> : std::formatter<std::string> {
  auto format(const srnp::Pair& pair, std::format_context& context) const {
    return std::formatter<std::string>::format(srnp::toString(pair), context);
  }
};

#endif /* PAIR_H_ */
