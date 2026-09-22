/*
  srnp_kernel.cpp - The process-wide node.

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

#include <srnp/srnp_kernel.h>
#include <srnp/srnp_print.h>

#include <charconv>
#include <cstdlib>
#include <format>
#include <string>

namespace srnp {
namespace {

/// How long to wait for the master to hand us an owner id.
constexpr std::chrono::seconds kReadyTimeout{10};

}  // namespace

std::shared_ptr<Server> KernelInstance::server_instance_;
std::shared_ptr<Client> KernelInstance::client_instance_;
std::shared_ptr<PairQueue> KernelInstance::pair_queue_;
std::shared_ptr<PairSpace> KernelInstance::pair_space_;
std::shared_ptr<asio::io_context> KernelInstance::io_context_;

bool ok() { return KernelInstance::client_instance_ != nullptr; }

void initialize(std::string_view master_ip, std::string_view master_port, int desired_owner_id,
                std::string_view node_name) {
  if (ok()) throw InitError("this process already has a running SRNP node");

  KernelInstance::pair_space_ = std::make_shared<PairSpace>();
  KernelInstance::pair_queue_ = std::make_shared<PairQueue>();
  KernelInstance::io_context_ = std::make_shared<asio::io_context>();

  try {
    KernelInstance::server_instance_ = std::make_shared<Server>(
        *KernelInstance::io_context_, std::string(master_ip), std::string(master_port),
        *KernelInstance::pair_space_, *KernelInstance::pair_queue_, desired_owner_id);
  } catch (const std::exception& e) {
    shutdown();
    throw InitError(std::format("could not reach the master at {}:{} ({})", master_ip,
                                master_port, e.what()));
  }

  KernelInstance::client_instance_ = std::make_shared<Client>(
      *KernelInstance::io_context_, "127.0.0.1",
      std::to_string(KernelInstance::server_instance_->getPort()), *KernelInstance::pair_space_,
      *KernelInstance::pair_queue_);

  if (!KernelInstance::client_instance_->waitUntilReady(kReadyTimeout)) {
    shutdown();
    throw InitError("the master never sent us an owner id");
  }

  SRNP_INFO("node \"{}\" started with owner id {}", node_name, getOwnerID());
}

void initialize(int argc, char* argv[]) {
  const char* master_ip = std::getenv("SRNP_MASTER_IP");
  const char* master_port = std::getenv("SRNP_MASTER_PORT");
  if (master_ip == nullptr || master_port == nullptr)
    throw InitError("set SRNP_MASTER_IP and SRNP_MASTER_PORT before starting a node");

  int desired_owner_id = kAnyOwner;
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) != "--owner-id") continue;
    if (i + 1 >= argc) throw InitError("--owner-id needs a number after it");

    const std::string_view value(argv[i + 1]);
    if (std::from_chars(value.data(), value.data() + value.size(), desired_owner_id).ec !=
        std::errc{})
      throw InitError(std::format("--owner-id got \"{}\", which is not a number", value));
  }

  initialize(master_ip, master_port, desired_owner_id, argc > 0 ? argv[0] : "srnp");
}

void shutdown() {
  if (KernelInstance::client_instance_) KernelInstance::client_instance_->close();
  if (KernelInstance::server_instance_) KernelInstance::server_instance_->stop();

  // The server joins its worker threads in stop(), so nothing is still
  // touching these by the time they go.
  KernelInstance::client_instance_.reset();
  KernelInstance::server_instance_.reset();
  KernelInstance::io_context_.reset();
  KernelInstance::pair_queue_.reset();
  KernelInstance::pair_space_.reset();
}

namespace {

/// Every call below needs a live node; this keeps the check in one place.
Client& client() {
  if (!KernelInstance::client_instance_)
    throw InitError("no SRNP node is running. Call initialize() first");
  return *KernelInstance::client_instance_;
}

}  // namespace

bool setPair(std::string_view key, std::string_view value, Pair::Type type) {
  return client().setPair(key, value, type);
}

bool setRemotePair(int owner, std::string_view key, std::string_view value, Pair::Type type) {
  return client().setRemotePair(owner, key, value, type);
}

bool setPairIndirectly(int metaowner, std::string_view metakey, std::string_view value) {
  return client().setPairIndirectly(metaowner, metakey, value);
}

bool setMetaPair(int meta_owner, std::string_view meta_key, int owner, std::string_view key) {
  return client().setMetaPair(meta_owner, meta_key, owner, key);
}

bool initMetaPair(int meta_owner, std::string_view meta_key) {
  return client().initMetaPair(meta_owner, meta_key);
}

bool removePair(std::string_view key) { return client().removePair(key); }

bool removeRemotePair(int owner, std::string_view key) {
  return client().removeRemotePair(owner, key);
}

std::optional<Pair> getPair(int owner, std::string_view key) {
  return client().getPair(owner, key);
}

std::optional<Pair> getPairIndirectly(int metaowner, std::string_view metakey) {
  return client().getPairIndirectly(metaowner, metakey);
}

std::vector<Pair> snapshotPairs() {
  client();  // Same check as everything else: there has to be a live node.

  std::vector<Pair> pairs;
  std::lock_guard lock(KernelInstance::pair_space_->mutex);
  for (const auto& [key, pair] : KernelInstance::pair_space_->getAllPairs()) {
    // Type::Invalid means a placeholder: subscribed or watched, but never
    // published, so there is no value to show.
    if (pair.getType() == Pair::Type::Invalid) continue;
    pairs.push_back(pair);
    pairs.back().callbacks_.clear();
  }
  return pairs;
}

std::map<int, ComponentInfo> components() { return client().components(); }

void printPairSpace() {
  client();  // Same check as everything else: there has to be a live node.
  KernelInstance::server_instance_->printPairSpace();
}

CallbackHandle registerCallback(int owner, std::string_view key,
                                Pair::CallbackFunction callback_fn) {
  return client().registerCallback(owner, key, std::move(callback_fn));
}

void cancelCallback(CallbackHandle handle) { client().cancelCallback(handle); }

SubscriptionHandle registerSubscription(int owner, std::string_view key) {
  return client().registerSubscription(owner, key);
}

SubscriptionHandle registerSubscription(std::string_view key) {
  return client().registerSubscription(key);
}

void cancelSubscription(SubscriptionHandle handle) { client().cancelSubscription(handle); }

void cancelSubscription(int owner, std::string_view key) {
  client().cancelSubscription(owner, key);
}

void cancelSubscription(std::string_view key) { client().cancelSubscription(key); }

int getOwnerID() { return client().ownerId(); }

}  // namespace srnp
