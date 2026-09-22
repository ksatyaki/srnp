/*
  CommMessages.h - Messages sent between a client and a server.

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

#ifndef COMMMESSAGES_H_
#define COMMMESSAGES_H_

#include <srnp/Pair.h>

#include <string>

namespace srnp {

/// Registers or cancels a subscription. A key of "*" means every pair the owner has.
struct Subscription {
  std::string key;
  /// Whose pair we want. kAnyOwner together with key "*" is the wildcard form.
  int owner_id = kAnyOwner;
  /// Who wants it.
  int subscriber = kAnyOwner;
  /// False cancels an existing subscription.
  bool registering = true;
};

/// Names the pair to delete. Sent to the owner, then on to its subscribers.
struct RemovePairRequest {
  int owner = kAnyOwner;
  std::string key;
};

/// The wildcard key, subscribing to everything a component owns.
inline constexpr std::string_view kWildcardKey = "*";

}  // namespace srnp

#endif /* COMMMESSAGES_H_ */
