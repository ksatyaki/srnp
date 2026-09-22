/*
  meta_pair_callback.cpp - Implementation of the meta-pair helpers.

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

#include <srnp/meta_pair_callback.hpp>
#include <srnp/srnp_print.h>

#include <charconv>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace srnp {
namespace {

/// What one tracked meta-pair currently points at, and the handles that
/// keep that target subscribed.
struct MetaTracker {
  Pair::CallbackFunction fn;

  SubscriptionHandle meta_subscription = kInvalidSubscriptionHandle;
  CallbackHandle meta_callback = kInvalidCallbackHandle;

  /// The pair the meta-pair names right now, empty when it names nothing.
  std::optional<PairKey> target;
  SubscriptionHandle target_subscription = kInvalidSubscriptionHandle;
  CallbackHandle target_callback = kInvalidCallbackHandle;
};

/// Callbacks run on io threads, so this guards every tracker.
std::mutex g_mutex;
std::map<PairKey, std::shared_ptr<MetaTracker>> g_trackers;

std::optional<PairKey> parseMetaValue(std::string_view value) {
  const auto parts = extractStrings(value);
  if (parts.size() != 3 || parts[0] != "META") return std::nullopt;

  int owner = 0;
  const auto& text = parts[1];
  if (std::from_chars(text.data(), text.data() + text.size(), owner).ec != std::errc{})
    return std::nullopt;

  // "(META -1 NULL)" is how a meta-pair says it points nowhere.
  if (owner == kAnyOwner && parts[2] == "NULL") return std::nullopt;
  return PairKey{owner, parts[2]};
}

/// Drops whatever the tracker currently follows. Call with g_mutex held.
void releaseTarget(MetaTracker& tracker) {
  if (tracker.target_callback != kInvalidCallbackHandle) {
    cancelCallback(tracker.target_callback);
    tracker.target_callback = kInvalidCallbackHandle;
  }
  if (tracker.target_subscription != kInvalidSubscriptionHandle) {
    cancelSubscription(tracker.target_subscription);
    tracker.target_subscription = kInvalidSubscriptionHandle;
  }
  tracker.target.reset();
}

/// Points the tracker at a new pair. Call with g_mutex held.
void acquireTarget(MetaTracker& tracker, const PairKey& target) {
  tracker.target = target;
  tracker.target_subscription = registerSubscription(target.owner, target.key);
  if (tracker.fn)
    tracker.target_callback = registerCallback(target.owner, target.key, tracker.fn);
}

/// Runs whenever a tracked meta-pair changes value.
void onMetaPairChanged(const Pair::ConstPtr& metapair) {
  const PairKey meta_key{metapair->getOwner(), metapair->getKey()};

  std::lock_guard lock(g_mutex);
  const auto it = g_trackers.find(meta_key);
  if (it == g_trackers.end()) return;  // Cancelled while this was in flight.

  MetaTracker& tracker = *it->second;
  const auto target = parseMetaValue(metapair->getValue());

  if (target == tracker.target) return;  // Points where it already pointed.

  releaseTarget(tracker);
  if (target) acquireTarget(tracker, *target);
}

void registerTracker(int meta_owner_id, std::string_view meta_pair_key,
                     Pair::CallbackFunction cb) {
  const PairKey meta_key{meta_owner_id, std::string(meta_pair_key)};

  std::lock_guard lock(g_mutex);
  if (const auto it = g_trackers.find(meta_key); it != g_trackers.end()) {
    // Already tracking it; just take the new callback.
    it->second->fn = std::move(cb);
    return;
  }

  auto tracker = std::make_shared<MetaTracker>();
  tracker->fn = std::move(cb);
  tracker->meta_subscription = registerSubscription(meta_owner_id, meta_pair_key);
  tracker->meta_callback = registerCallback(meta_owner_id, meta_pair_key, onMetaPairChanged);

  g_trackers.emplace(meta_key, std::move(tracker));
}

}  // namespace

void registerMetaCallback(int meta_owner_id, std::string_view meta_pair_key,
                          Pair::CallbackFunction cb) {
  registerTracker(meta_owner_id, meta_pair_key, std::move(cb));
}

void registerMetaSubscription(int meta_owner_id, std::string_view meta_pair_key) {
  registerTracker(meta_owner_id, meta_pair_key, nullptr);
}

void cancelMetaCallback(int meta_owner_id, std::string_view meta_pair_key) {
  const PairKey meta_key{meta_owner_id, std::string(meta_pair_key)};

  std::lock_guard lock(g_mutex);
  const auto it = g_trackers.find(meta_key);
  if (it == g_trackers.end()) {
    SRNP_DEBUG("nothing tracking the meta-pair [{}] {}", meta_owner_id, meta_pair_key);
    return;
  }

  MetaTracker& tracker = *it->second;
  releaseTarget(tracker);
  cancelSubscription(tracker.meta_subscription);
  cancelCallback(tracker.meta_callback);

  g_trackers.erase(it);
}

void cancelMetaSubscription(int meta_owner_id, std::string_view meta_pair_key) {
  cancelMetaCallback(meta_owner_id, meta_pair_key);
}

}  // namespace srnp
