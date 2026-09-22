/*
  PairSpace.cpp - Implementation of PairSpace.

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
#include <srnp/PairSpace.h>
#include <srnp/srnp_print.h>

#include <algorithm>
#include <iostream>

namespace srnp {

PairEntry* PairSpace::find(int owner, std::string_view key) {
  const auto it = pairs_.find(PairKeyView{owner, key});
  return it == pairs_.end() ? nullptr : &it->second;
}

const PairEntry* PairSpace::find(int owner, std::string_view key) const {
  const auto it = pairs_.find(PairKeyView{owner, key});
  return it == pairs_.end() ? nullptr : &it->second;
}

std::optional<Pair> PairSpace::copyOf(int owner, std::string_view key) const {
  if (const PairEntry* entry = find(owner, key)) return entry->pair;
  return std::nullopt;
}

PairEntry& PairSpace::addPair(const Pair& pair) {
  const auto it = pairs_.find(PairKeyView{pair.getOwner(), pair.getKey()});

  if (it == pairs_.end()) {
    auto [added, _] = pairs_.emplace(PairKey{pair.getOwner(), pair.getKey()}, PairEntry{pair, {}, {}});
    added->second.pair.setWriteTime(Clock::now());
    added->second.subscribers = u_subscribers_;
    return added->second;
  }

  PairEntry& existing = it->second;
  existing.pair.setType(pair.getType());
  existing.pair.setValue(pair.getValue());
  existing.pair.setWriteTime(Clock::now());
  return existing;
}

void PairSpace::removePair(int owner, std::string_view key) {
  // Heterogeneous erase is C++23, so look the iterator up first.
  const auto it = pairs_.find(PairKeyView{owner, key});
  if (it == pairs_.end()) return;

  PairEntry& entry = it->second;
  if (entry.subscribers.empty() && entry.callbacks.empty()) {
    pairs_.erase(it);
    return;
  }

  entry.pair.setValue("");
  entry.pair.setType(Pair::Type::Invalid);
}

CallbackHandle PairSpace::addCallback(int owner, std::string_view key,
                                      Pair::CallbackFunction callback_fn) {
  const auto handle = ++next_callback_handle_;
  callback_owners_.emplace(handle, PairKey{owner, std::string(key)});

  PairEntry* entry = find(owner, key);
  if (entry == nullptr) {
    // Register against a placeholder so the callback survives until the
    // pair is actually published.
    entry = &addPair(Pair(owner, std::string(key), "", Pair::Type::Invalid));
  }

  entry->callbacks.emplace(
      handle, std::make_shared<const Pair::CallbackFunction>(std::move(callback_fn)));
  return handle;
}

CallbackHandle PairSpace::addCallbackToAll(Pair::CallbackFunction callback_fn) {
  SRNP_DEBUG("adding a callback covering every pair");
  const auto handle = ++next_callback_handle_;
  universal_callbacks_.emplace(
      handle, std::make_shared<const Pair::CallbackFunction>(std::move(callback_fn)));
  return handle;
}

std::vector<std::shared_ptr<const Pair::CallbackFunction>> PairSpace::universalCallbacks()
    const {
  std::vector<std::shared_ptr<const Pair::CallbackFunction>> out;
  out.reserve(universal_callbacks_.size());
  for (const auto& [handle, callback] : universal_callbacks_) out.push_back(callback);
  return out;
}

void PairSpace::removeCallback(CallbackHandle handle) {
  const auto owner_it = callback_owners_.find(handle);
  if (owner_it == callback_owners_.end()) {
    // Not registered against a pair, so it is either universal or unknown.
    if (universal_callbacks_.erase(handle) == 0)
      SRNP_WARN("no callback registered under handle {}", handle);
    return;
  }

  if (PairEntry* entry = find(owner_it->second.owner, owner_it->second.key))
    entry->callbacks.erase(handle);

  callback_owners_.erase(owner_it);
}

void PairSpace::addSubscription(int owner, std::string_view key, int subscriber) {
  PairEntry* entry = find(owner, key);
  if (entry == nullptr) {
    // Same idea as addCallback: hold the subscription until the pair exists.
    entry = &addPair(Pair(owner, std::string(key), "", Pair::Type::Invalid));
  }

  if (std::ranges::find(entry->subscribers, subscriber) != entry->subscribers.end()) {
    SRNP_WARN("{} is already subscribed to [{}] {}", subscriber, owner, key);
    return;
  }
  entry->subscribers.push_back(subscriber);
}

void PairSpace::removeSubscription(int owner, std::string_view key, int subscriber) {
  PairEntry* entry = find(owner, key);
  if (entry == nullptr) {
    SRNP_WARN("cannot unsubscribe {} from [{}] {}: no such pair", subscriber, owner, key);
    return;
  }

  if (std::erase(entry->subscribers, subscriber) == 0)
    SRNP_WARN("{} was not subscribed to [{}] {}", subscriber, owner, key);
}

void PairSpace::addSubscriptionToAll(int subscriber) {
  if (std::ranges::find(u_subscribers_, subscriber) != u_subscribers_.end()) {
    SRNP_WARN("{} already has a subscription to everything", subscriber);
    return;
  }

  SRNP_INFO("{} subscribed to every pair", subscriber);
  u_subscribers_.push_back(subscriber);

  for (auto& [key, entry] : pairs_) {
    if (std::ranges::find(entry.subscribers, subscriber) == entry.subscribers.end())
      entry.subscribers.push_back(subscriber);
  }
}

void PairSpace::removeSubscriptionToAll(int subscriber) {
  std::erase(u_subscribers_, subscriber);

  // Also drops per-pair subscriptions, so this doubles as the cleanup when
  // a component disconnects.
  for (auto& [key, entry] : pairs_) std::erase(entry.subscribers, subscriber);
}

void PairSpace::printPairSpace() const {
  std::cout << "--- all pairs (" << pairs_.size() << ") ---\n";
  for (const auto& [key, entry] : pairs_) std::cout << "  " << entry.pair << '\n';
  std::cout << "---\n" << std::flush;
}

}  // namespace srnp
