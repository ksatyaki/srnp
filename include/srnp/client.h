/*
  client.h - Client is what an application directly uses.

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

#ifndef SRNP_CLIENT_H_
#define SRNP_CLIENT_H_

#include <srnp/Pair.h>
#include <srnp/local_client.h>
#include <srnp/PairSpace.h>
#include <srnp/msgs/CommMessages.h>
#include <srnp/msgs/MasterMessages.h>
#include <srnp/session.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace srnp {

class Client;

/// How long to wait before retrying a connection to another component.
inline constexpr std::chrono::seconds kReconnectDelay{10};

/**
 * One outbound connection, to another component's server. Write-only: we
 * push pairs and subscriptions and never read a reply. Our own server is in
 * this process and is not reached through one of these.
 */
class ClientSession : public std::enable_shared_from_this<ClientSession> {
 public:
  ClientSession(asio::io_context& io, std::string host, std::string port, Client& client,
                int endpoint_owner_id);

  /// Connects, replays our subscriptions, then idles. Retries until it
  /// succeeds or we close.
  asio::awaitable<void> run();

  /// `coalesce_key` marks a pair update droppable and names the key it
  /// carries, so a backed-up queue can replace it rather than grow. Leave it
  /// unset for anything that is not last-value-wins.
  void send(std::vector<std::byte> frame,
            std::optional<PairKey> coalesce_key = std::nullopt);
  void close();

  Strand& strand() { return strand_; }
  int endpointOwner() const { return endpoint_owner_id_; }

 private:
  asio::awaitable<bool> connect();

  /// Replays our subscriptions to a component we just connected to.
  void sendSubscriptionsFor(int owner);

  asio::io_context& io_;
  /// Owned here rather than by the channel, because run() needs a strand
  /// to start on before there is a connection to make a channel from.
  Strand strand_;
  std::string host_;
  std::string port_;
  Client& client_;
  int endpoint_owner_id_;

  std::mutex channel_mutex_;
  FrameChannelPtr channel_;
  /// Frames sent before the connection came up. Without this, anything
  /// published in the moment after learning about a component is lost.
  struct Pending {
    std::vector<std::byte> bytes;
    std::optional<PairKey> coalesce_key;
  };
  std::deque<Pending> pending_;
  std::atomic<bool> closing_{false};
};

using ClientSessionPtr = std::shared_ptr<ClientSession>;

class Client : public LocalClient {
 public:
  Client(asio::io_context& io, PairSpace& pair_space);
  ~Client() override;

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  /// True once the master has told us our owner id.
  bool ready() const { return ready_.load(std::memory_order_acquire); }

  /// Blocks until ready(), or until the timeout runs out.
  bool waitUntilReady(std::chrono::milliseconds timeout);

  int ownerId() const { return owner_id_.load(std::memory_order_relaxed); }

  [[nodiscard]] bool setPair(std::string_view key, std::string_view value,
                             Pair::Type type = Pair::Type::String);
  [[nodiscard]] bool setRemotePair(int owner, std::string_view key, std::string_view value,
                                   Pair::Type type = Pair::Type::String);

  /// Deletes one of our own pairs. Subscribers are told it is gone.
  [[nodiscard]] bool removePair(std::string_view key);
  /// Deletes a pair on another component. Sending it to ourselves is the
  /// same as removePair.
  [[nodiscard]] bool removeRemotePair(int owner, std::string_view key);

  /// Follows a meta-pair to the pair it points at, then sets that one.
  [[nodiscard]] bool setPairIndirectly(int metaowner, std::string_view metakey,
                                       std::string_view value);

  /// Points a meta-pair at <owner, key>.
  [[nodiscard]] bool setMetaPair(int meta_owner, std::string_view meta_key, int owner,
                                 std::string_view key);
  /// Creates a meta-pair that points nowhere yet.
  [[nodiscard]] bool initMetaPair(int meta_owner, std::string_view meta_key);

  std::optional<Pair> getPair(int owner, std::string_view key);
  std::optional<Pair> getPairIndirectly(int metaowner, std::string_view metakey);

  CallbackHandle registerCallback(int owner, std::string_view key,
                                  Pair::CallbackFunction callback_fn);
  void cancelCallback(CallbackHandle handle);

  /// Subscribes to one component's pair, or with kAnyOwner to that key on
  /// every component. Returns kInvalidSubscriptionHandle if already subscribed.
  SubscriptionHandle registerSubscription(int owner, std::string_view key);
  SubscriptionHandle registerSubscription(std::string_view key);

  void cancelSubscription(SubscriptionHandle handle);
  void cancelSubscription(int owner, std::string_view key);
  void cancelSubscription(std::string_view key);

  /// Every other component the master has told us about, by owner id.
  /// Ourselves excluded: the master never lists us to ourselves.
  std::map<int, ComponentInfo> components() const;

  void close();

  // LocalClient. Called by our server, from an io thread.
  void fanOutPair(const Pair& pair, std::span<const int> subscribers) override;
  void fanOutRemoval(const RemovePairRequest& request,
                     std::span<const int> subscribers) override;
  void onWelcome(const MasterMessage& welcome) override;
  void onComponentUpdate(const UpdateComponents& update) override;

 private:
  friend class ClientSession;

  /// One subscription we hold, remembered so it can be replayed to
  /// components that connect later.
  struct SubscriptionRecord {
    int owner = kAnyOwner;
    std::string key;
  };

  void addSession(const ComponentInfo& component);
  void removeSession(int owner);

  ClientSessionPtr sessionFor(int owner) const;
  std::vector<ClientSessionPtr> allSessions() const;

  /// Runs the callbacks registered against a pair, on callback_strand_ so
  /// two rapid publishes cannot deliver out of order across the io threads.
  void notifyLocal(Pair::ConstPtr snapshot,
                   std::vector<std::shared_ptr<const Pair::CallbackFunction>> callbacks);

  /// The subscriptions that apply to one component, wildcard ones included.
  std::vector<SubscriptionRecord> subscriptionsFor(int owner) const;

  bool sendSubscription(int owner, std::string_view key, bool registering);

  asio::io_context& io_;
  PairSpace& pair_space_;

  /// Local callbacks run here, one at a time and in publish order. They used
  /// to run on the single loopback session's strand, which gave the same
  /// ordering by accident; this states it instead.
  Strand callback_strand_;

  std::atomic<int> owner_id_{kAnyOwner};
  std::atomic<bool> ready_{false};

  /// Only for waiters that want a timeout; ready() itself stays lock-free.
  std::mutex ready_mutex_;
  std::condition_variable ready_changed_;

  mutable std::mutex state_mutex_;
  std::map<int, ClientSessionPtr> sessions_;
  std::map<int, ComponentInfo> components_;
  /// Every subscription we hold, by handle. Replaces the four maps this
  /// used to keep in parallel.
  std::map<SubscriptionHandle, SubscriptionRecord> subscriptions_;
  SubscriptionHandle next_subscription_handle_ = kInvalidSubscriptionHandle;
};

/// Splits "(META 1234 key)" into its three words. Public because the
/// meta-pair helpers parse the same format.
std::vector<std::string> extractStrings(std::string_view text);

}  // namespace srnp

#endif /* SRNP_CLIENT_H_ */
