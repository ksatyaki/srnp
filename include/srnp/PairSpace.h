/*
  PairSpace.h - The space of all pairs.

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
#ifndef PAIRSPACE_H_
#define PAIRSPACE_H_

#include <srnp/Pair.h>

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace srnp {

/**
 * Holds every pair this component knows about, its own and the ones it
 * subscribed to. Callers must hold `mutex` for the whole of any operation.
 */
class PairSpace {
 public:
  /// Runs for every pair that changes, on top of any per-pair callbacks.
  Pair::CallbackFunction u_callback_;

  /// Guards everything below. Public because callers often need to hold it
  /// across several calls, for example a lookup followed by an update.
  std::mutex mutex;

  using Storage = std::map<PairKey, Pair, PairKeyLess>;

  /// Null when there is no such pair. Valid only while the lock is held.
  Pair* find(int owner, std::string_view key);
  const Pair* find(int owner, std::string_view key) const;

  /// A detached copy, safe to use after releasing the lock.
  std::optional<Pair> copyOf(int owner, std::string_view key) const;

  /**
   * Deletes the pair. Subscribers and callbacks are ours, not the value's,
   * so anything still registered leaves the entry behind as the same
   * Invalid placeholder addSubscription() creates for a key nobody has
   * published yet. Re-publishing the key then reaches them again.
   */
  void removePair(int owner, std::string_view key);

  const Storage& getAllPairs() const { return pairs_; }

  /**
   * Adds the pair, or updates the value and type of an existing one.
   * An update deliberately keeps the existing subscribers and callbacks:
   * they belong to this component, not to whoever sent the new value.
   */
  Pair& addPair(const Pair& pair);

  /// Creates a placeholder pair if the key isn't published yet, so the
  /// subscription is already in place when the first value arrives.
  void addSubscription(int owner, std::string_view key, int subscriber);
  void removeSubscription(int owner, std::string_view key, int subscriber);

  /// Subscribes to every pair we hold now and every one added later.
  void addSubscriptionToAll(int subscriber);
  void removeSubscriptionToAll(int subscriber);

  CallbackHandle addCallback(int owner, std::string_view key, Pair::CallbackFunction callback_fn);
  void removeCallback(CallbackHandle handle);
  void addCallbackToAll(Pair::CallbackFunction callback_fn);

  void printPairSpace() const;

 private:
  Storage pairs_;

  /// Added to every new pair, so late arrivals inherit wildcard subscriptions.
  std::vector<int> u_subscribers_;

  /// Lets removeCallback find the pair a handle was registered against.
  std::map<CallbackHandle, PairKey> callback_owners_;

  CallbackHandle next_callback_handle_ = kInvalidCallbackHandle;
};

}  // namespace srnp

#endif /* PAIRSPACE_H_ */
