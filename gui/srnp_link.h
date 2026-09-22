/*
  srnp_link.h - Keeps the GUI attached to an srnp node.

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

#ifndef SRNP_GUI_SRNP_LINK_H_
#define SRNP_GUI_SRNP_LINK_H_

#include <srnp/srnp_kernel.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace srnp::gui {

/**
 * Starts an srnp node and keeps it started. initialize() blocks for up to
 * ten seconds, which would freeze the window, so it runs on a worker thread
 * and the render thread reads the state each frame.
 */
class SrnpLink {
 public:
  enum class State { Disconnected, Connecting, Connected };

  /// Starts trying immediately. Retries every three seconds until it works.
  SrnpLink(std::string master_ip, std::string master_port, int desired_owner_id);

  /// Joins the worker before shutting the node down, so nothing is still
  /// calling into srnp while it is being torn down.
  ~SrnpLink();

  SrnpLink(const SrnpLink&) = delete;
  SrnpLink& operator=(const SrnpLink&) = delete;

  State state() const;

  /// Why the last attempt failed. Empty until one has.
  std::string lastError() const;

  /// "ip:port", for the banner.
  std::string address() const;

  /// True while our standing wildcard subscription is in place. Turning it
  /// off is what lets the per-pair toggles mean anything.
  bool subscribedToAll() const { return subscribed_to_all_; }
  void setSubscribedToAll(bool on);

 private:
  void connectLoop(std::stop_token stop);

  /// Subscribes to everything every component owns. Without this, no other
  /// component's pairs ever reach our pair space.
  void subscribeToAll();

  std::string master_ip_;
  std::string master_port_;
  int desired_owner_id_;

  mutable std::mutex mutex_;
  State state_ = State::Disconnected;
  std::string last_error_;

  /// Read every frame by the render thread, set by the worker on connect.
  std::atomic<bool> subscribed_to_all_{false};
  SubscriptionHandle wildcard_ = kInvalidSubscriptionHandle;

  /// Last, so it is the first thing destroyed and never outlives what it uses.
  std::jthread worker_;
};

}  // namespace srnp::gui

#endif  // SRNP_GUI_SRNP_LINK_H_
