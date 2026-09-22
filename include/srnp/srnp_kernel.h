/*
  srnp_kernel.h
  A Wrapper for all the SRNP tools.

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

#ifndef SRNP_KERNEL_H_
#define SRNP_KERNEL_H_

#include <srnp/client.h>
#include <srnp/server.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace srnp {

/// Thrown by initialize() when the node cannot start.
struct InitError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// The one node this process runs. Everything below operates on it.
class KernelInstance {
 public:
  static std::shared_ptr<Server> server_instance_;
  static std::shared_ptr<Client> client_instance_;
  static std::shared_ptr<PairQueue> pair_queue_;
  static std::shared_ptr<PairSpace> pair_space_;
  static std::shared_ptr<asio::io_context> io_context_;
};

/**
 * Starts the node. Reads SRNP_MASTER_IP and SRNP_MASTER_PORT from the
 * environment, and accepts "--owner-id <n>" to ask for a specific id.
 * Throws InitError instead of exiting, so a caller can recover.
 */
void initialize(int argc, char* argv[]);

/// Same, for a caller that would rather pass the master's address directly.
void initialize(std::string_view master_ip, std::string_view master_port,
                int desired_owner_id = kAnyOwner, std::string_view node_name = "srnp");

/// Stops the node and releases everything. Safe to call more than once.
void shutdown();

/// True between a successful initialize() and shutdown().
bool ok();

bool setPair(std::string_view key, std::string_view value,
             Pair::Type type = Pair::Type::String);
bool setRemotePair(int owner, std::string_view key, std::string_view value,
                   Pair::Type type = Pair::Type::String);
bool setPairIndirectly(int metaowner, std::string_view metakey, std::string_view value);

bool setMetaPair(int meta_owner, std::string_view meta_key, int owner, std::string_view key);
bool initMetaPair(int meta_owner, std::string_view meta_key);

std::optional<Pair> getPair(int owner, std::string_view key);
std::optional<Pair> getPairIndirectly(int metaowner, std::string_view metakey);

void printPairSpace();

CallbackHandle registerCallback(int owner, std::string_view key,
                                Pair::CallbackFunction callback_fn);
void cancelCallback(CallbackHandle handle);

/// Subscribes to one component's pair, or with kAnyOwner to that key
/// wherever it appears.
SubscriptionHandle registerSubscription(int owner, std::string_view key);
SubscriptionHandle registerSubscription(std::string_view key);

void cancelSubscription(SubscriptionHandle handle);
void cancelSubscription(int owner, std::string_view key);
void cancelSubscription(std::string_view key);

int getOwnerID();

}  // namespace srnp

#endif /* SRNP_KERNEL_H_ */
