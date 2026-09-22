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

#include <memory>

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
/**
 * A pair, plus the bookkeeping this component keeps about it.
 *
 * The two are separate because they belong to different owners: the value
 * came from whoever published it, while the subscriber list and the
 * callbacks are ours. Keeping them in one object meant every update copied
 * a callback map nobody had asked for.
 */
struct PairEntry {
  Pair pair;
  /// Owner ids this pair is forwarded to on every update.
  std::vector<int> subscribers;
  /// Held by shared_ptr so notifying is a refcount bump, not a copy of
  /// every std::function on the hot path.
  std::map<CallbackHandle, std::shared_ptr<const Pair::CallbackFunction>> callbacks;
};

class PairSpace {
 public:
  /// Guards everything below. Public because callers often need to hold it
  /// across several calls, for example a lookup followed by an update.
  std::mutex mutex;

  using Storage = std::map<PairKey, PairEntry, PairKeyLess>;

  /// Null when there is no such pair. Valid only while the lock is held.
  PairEntry* find(int owner, std::string_view key);
  const PairEntry* find(int owner, std::string_view key) const;

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
  PairEntry& addPair(const Pair& pair);

  /// Creates a placeholder pair if the key isn't published yet, so the
  /// subscription is already in place when the first value arrives.
  void addSubscription(int owner, std::string_view key, int subscriber);
  void removeSubscription(int owner, std::string_view key, int subscriber);

  /// Subscribes to every pair we hold now and every one added later.
  void addSubscriptionToAll(int subscriber);
  void removeSubscriptionToAll(int subscriber);

  CallbackHandle addCallback(int owner, std::string_view key, Pair::CallbackFunction callback_fn);
  void removeCallback(CallbackHandle handle);

  /// A callback that runs for every pair. Returns a handle so it can be
  /// cancelled; more than one may be registered at a time.
  CallbackHandle addCallbackToAll(Pair::CallbackFunction callback_fn);

  /// Every universal callback, for notifying outside the lock.
  std::vector<std::shared_ptr<const Pair::CallbackFunction>> universalCallbacks() const;

  void printPairSpace() const;

 private:
  Storage pairs_;

  /// Added to every new pair, so late arrivals inherit wildcard subscriptions.
  std::vector<int> u_subscribers_;

  /// Lets removeCallback find the pair a handle was registered against.
  /// A handle with no entry here is a universal one.
  std::map<CallbackHandle, PairKey> callback_owners_;

  /// Callbacks covering every pair, by handle. A map rather than the single
  /// slot this used to be: registering a second one silently replaced the
  /// first, and neither could be cancelled.
  std::map<CallbackHandle, std::shared_ptr<const Pair::CallbackFunction>> universal_callbacks_;

  CallbackHandle next_callback_handle_ = kInvalidCallbackHandle;
};

}  // namespace srnp

#endif /* PAIRSPACE_H_ */
