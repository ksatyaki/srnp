/*
  local_client.h - What our server needs from the client in the same process.

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

#ifndef SRNP_LOCAL_CLIENT_H_
#define SRNP_LOCAL_CLIENT_H_

#include <srnp/Pair.h>
#include <srnp/msgs/CommMessages.h>
#include <srnp/msgs/MasterMessages.h>

#include <span>

namespace srnp {

/**
 * The client half of a component, as its server sees it.
 *
 * This used to be a loopback TCP connection: the server sent a frame to
 * 127.0.0.1 and its own client read it back. This interface is what is left
 * once that socket is gone. Kept deliberately narrow so the two halves stay
 * separable, which is what makes them testable apart.
 *
 * Every method is called from an io thread and must not block.
 */
class LocalClient {
 public:
  virtual ~LocalClient() = default;

  /// Sends the pair on to each subscriber. Ours to forward because we own
  /// the connections to the other components; the pair space is the server's.
  virtual void fanOutPair(const Pair& pair, std::span<const int> subscribers) = 0;

  /// Tells each subscriber that one of our pairs is gone.
  virtual void fanOutRemoval(const RemovePairRequest& request,
                             std::span<const int> subscribers) = 0;

  /// Our owner id and the components the master already knew about. Sent
  /// once, as soon as the master replies.
  virtual void onWelcome(const MasterMessage& welcome) = 0;

  /// A component appeared or disappeared.
  virtual void onComponentUpdate(const UpdateComponents& update) = 0;
};

}  // namespace srnp

#endif /* SRNP_LOCAL_CLIENT_H_ */
