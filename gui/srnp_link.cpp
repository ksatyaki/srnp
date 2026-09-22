/*
  srnp_link.cpp - Implementation of SrnpLink.

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

#include "srnp_link.h"

#include <srnp/msgs/CommMessages.h>
#include <srnp/srnp_print.h>

#include <chrono>
#include <condition_variable>
#include <format>

namespace srnp::gui {
namespace {

constexpr auto kRetryDelay = std::chrono::seconds(3);

}  // namespace

SrnpLink::SrnpLink(std::string master_ip, std::string master_port, int desired_owner_id)
    : master_ip_(std::move(master_ip)),
      master_port_(std::move(master_port)),
      desired_owner_id_(desired_owner_id) {
  worker_ = std::jthread([this](std::stop_token stop) { connectLoop(stop); });
}

SrnpLink::~SrnpLink() {
  worker_.request_stop();
  if (worker_.joinable()) worker_.join();
  if (ok()) shutdown();
}

void SrnpLink::connectLoop(std::stop_token stop) {
  std::mutex sleep_mutex;
  std::condition_variable_any wake;

  while (!stop.stop_requested()) {
    {
      std::lock_guard lock(mutex_);
      state_ = State::Connecting;
    }

    try {
      initialize(master_ip_, master_port_, desired_owner_id_, "pairview");
      subscribeToAll();

      std::lock_guard lock(mutex_);
      state_ = State::Connected;
      last_error_.clear();
      SRNP_INFO("pairview connected as owner {}", getOwnerID());
      return;
    } catch (const std::exception& e) {
      std::lock_guard lock(mutex_);
      state_ = State::Disconnected;
      last_error_ = e.what();
    }

    // Interruptible, so closing the window does not wait out the delay.
    std::unique_lock lock(sleep_mutex);
    wake.wait_for(lock, stop, kRetryDelay, [&] { return stop.stop_requested(); });
  }
}

void SrnpLink::subscribeToAll() {
  wildcard_ = registerSubscription(kAnyOwner, kWildcardKey);
  subscribed_to_all_ = true;
}

void SrnpLink::setSubscribedToAll(bool on) {
  if (on == subscribed_to_all_ || state() != State::Connected) return;

  if (on) {
    subscribeToAll();
    return;
  }

  cancelSubscription(wildcard_);
  wildcard_ = kInvalidSubscriptionHandle;
  subscribed_to_all_ = false;
}

SrnpLink::State SrnpLink::state() const {
  std::lock_guard lock(mutex_);
  return state_;
}

std::string SrnpLink::lastError() const {
  std::lock_guard lock(mutex_);
  return last_error_;
}

std::string SrnpLink::address() const { return std::format("{}:{}", master_ip_, master_port_); }

}  // namespace srnp::gui
