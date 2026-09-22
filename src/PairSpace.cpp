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

Pair* PairSpace::find(int owner, std::string_view key) {
  const auto it = pairs_.find(PairKeyView{owner, key});
  return it == pairs_.end() ? nullptr : &it->second;
}

const Pair* PairSpace::find(int owner, std::string_view key) const {
  const auto it = pairs_.find(PairKeyView{owner, key});
  return it == pairs_.end() ? nullptr : &it->second;
}

std::optional<Pair> PairSpace::copyOf(int owner, std::string_view key) const {
  if (const Pair* pair = find(owner, key)) return *pair;
  return std::nullopt;
}

Pair& PairSpace::addPair(const Pair& pair) {
  const auto it = pairs_.find(PairKeyView{pair.getOwner(), pair.getKey()});

  if (it == pairs_.end()) {
    auto [added, _] = pairs_.emplace(PairKey{pair.getOwner(), pair.getKey()}, pair);
    added->second.setWriteTime(Clock::now());
    added->second.subscribers_ = u_subscribers_;
    return added->second;
  }

  Pair& existing = it->second;
  existing.setType(pair.getType());
  existing.setValue(pair.getValue());
  existing.setWriteTime(Clock::now());
  return existing;
}

void PairSpace::removePair(int owner, std::string_view key) {
  // Heterogeneous erase is C++23, so look the iterator up first.
  const auto it = pairs_.find(PairKeyView{owner, key});
  if (it == pairs_.end()) return;

  Pair& pair = it->second;
  if (pair.subscribers_.empty() && pair.callbacks_.empty()) {
    pairs_.erase(it);
    return;
  }

  pair.setValue("");
  pair.setType(Pair::Type::Invalid);
}

CallbackHandle PairSpace::addCallback(int owner, std::string_view key,
                                      Pair::CallbackFunction callback_fn) {
  const auto handle = ++next_callback_handle_;
  callback_owners_.emplace(handle, PairKey{owner, std::string(key)});

  Pair* pair = find(owner, key);
  if (pair == nullptr) {
    // Register against a placeholder so the callback survives until the
    // pair is actually published.
    pair = &addPair(Pair(owner, std::string(key), "", Pair::Type::Invalid));
  }

  pair->callbacks_.emplace(handle, std::move(callback_fn));
  return handle;
}

void PairSpace::addCallbackToAll(Pair::CallbackFunction callback_fn) {
  SRNP_DEBUG("adding a callback covering every pair");
  u_callback_ = std::move(callback_fn);
}

void PairSpace::removeCallback(CallbackHandle handle) {
  const auto owner_it = callback_owners_.find(handle);
  if (owner_it == callback_owners_.end()) {
    SRNP_WARN("no callback registered under handle {}", handle);
    return;
  }

  if (Pair* pair = find(owner_it->second.owner, owner_it->second.key))
    pair->callbacks_.erase(handle);

  callback_owners_.erase(owner_it);
}

void PairSpace::addSubscription(int owner, std::string_view key, int subscriber) {
  Pair* pair = find(owner, key);
  if (pair == nullptr) {
    // Same idea as addCallback: hold the subscription until the pair exists.
    pair = &addPair(Pair(owner, std::string(key), "", Pair::Type::Invalid));
  }

  if (std::ranges::find(pair->subscribers_, subscriber) != pair->subscribers_.end()) {
    SRNP_WARN("{} is already subscribed to [{}] {}", subscriber, owner, key);
    return;
  }
  pair->subscribers_.push_back(subscriber);
}

void PairSpace::removeSubscription(int owner, std::string_view key, int subscriber) {
  Pair* pair = find(owner, key);
  if (pair == nullptr) {
    SRNP_WARN("cannot unsubscribe {} from [{}] {}: no such pair", subscriber, owner, key);
    return;
  }

  if (std::erase(pair->subscribers_, subscriber) == 0)
    SRNP_WARN("{} was not subscribed to [{}] {}", subscriber, owner, key);
}

void PairSpace::addSubscriptionToAll(int subscriber) {
  if (std::ranges::find(u_subscribers_, subscriber) != u_subscribers_.end()) {
    SRNP_WARN("{} already has a subscription to everything", subscriber);
    return;
  }

  SRNP_INFO("{} subscribed to every pair", subscriber);
  u_subscribers_.push_back(subscriber);

  for (auto& [key, pair] : pairs_) {
    if (std::ranges::find(pair.subscribers_, subscriber) == pair.subscribers_.end())
      pair.subscribers_.push_back(subscriber);
  }
}

void PairSpace::removeSubscriptionToAll(int subscriber) {
  std::erase(u_subscribers_, subscriber);

  // Also drops per-pair subscriptions, so this doubles as the cleanup when
  // a component disconnects.
  for (auto& [key, pair] : pairs_) std::erase(pair.subscribers_, subscriber);
}

void PairSpace::printPairSpace() const {
  std::cout << "--- all pairs (" << pairs_.size() << ") ---\n";
  for (const auto& [key, pair] : pairs_) std::cout << "  " << pair << '\n';
  std::cout << "---\n" << std::flush;
}

}  // namespace srnp
