/*
  MasterMessages.h - Messages exchanged with the master.

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

#ifndef MASTER_MESSAGES_H_
#define MASTER_MESSAGES_H_

#include <srnp/Pair.h>

#include <string>
#include <vector>

namespace srnp {

/// Sent by a component's server to the master as soon as it connects.
struct IndicatePresence {
  /// The port our server listens on, so other components can reach us.
  std::string port;
  /// Ask to keep a specific owner id instead of letting the master pick.
  bool force_owner_id = false;
  int owner_id = kAnyOwner;
};

/// Where to reach one component.
struct ComponentInfo {
  int owner = kAnyOwner;
  std::string ip;
  std::string port;
};

/// Tells a component that another one appeared or disappeared.
struct UpdateComponents {
  enum class Operation : std::uint8_t { Add = 0, Remove };

  Operation operation = Operation::Add;
  ComponentInfo component;
};

/// The master's reply: our owner id, plus everyone who registered before us.
struct MasterMessage {
  int owner = kAnyOwner;
  std::vector<ComponentInfo> all_components;
};

}  // namespace srnp

#endif /* MASTER_MESSAGES_H_ */
