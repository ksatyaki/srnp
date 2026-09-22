/*
  PairQueue.h - Hands a pair to the local server without putting it on the
  socket.

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

#ifndef PAIRQUEUE_H_
#define PAIRQUEUE_H_

#include <srnp/Pair.h>

#include <mutex>
#include <optional>
#include <queue>

namespace srnp {

/**
 * A queue paired with an empty notification frame. The sender pushes and
 * sends under the same lock, so the reader always pops the pair that
 * belongs to the frame it just received.
 */
class PairHandoff {
 public:
  /// Hold this across push and the send that announces it. Without that,
  /// another thread could push and send in between, and the reader would
  /// pop the wrong pair.
  [[nodiscard]] std::unique_lock<std::mutex> lock() { return std::unique_lock(mutex_); }

  /// Call with lock() held.
  void push(Pair pair) { queue_.push(std::move(pair)); }

  /// Empty only if a frame arrived without its pair, which means a bug.
  std::optional<Pair> pop() {
    std::lock_guard guard(mutex_);
    if (queue_.empty()) return std::nullopt;
    Pair pair = std::move(queue_.front());
    queue_.pop();
    return pair;
  }

 private:
  std::mutex mutex_;
  std::queue<Pair> queue_;
};

struct PairQueue {
  /// Pairs going from our client to our own server.
  PairHandoff outgoing;

  /// Pairs our server wants the client to forward to subscribers.
  PairHandoff updates;
};

}  // namespace srnp

#endif /* PAIRQUEUE_H_ */
