/*
  meta_pair_callback.hpp - Callbacks that follow a meta-pair.

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

#ifndef SRNP_META_PAIR_CALLBACK_HPP_
#define SRNP_META_PAIR_CALLBACK_HPP_

#include <srnp/srnp_kernel.h>

#include <string>
#include <string_view>

namespace srnp {

/**
 * A meta-pair holds "(META owner key)", naming another pair. Registering
 * here follows that pointer: when the meta-pair changes to name a
 * different target, the subscription and callback move with it.
 *
 * @param cb Left empty to only track the target, without a callback.
 */
void registerMetaCallback(int meta_owner_id, std::string_view meta_pair_key,
                          Pair::CallbackFunction cb);

/// Tracks the target of a meta-pair without running a callback on it.
void registerMetaSubscription(int meta_owner_id, std::string_view meta_pair_key);

/// Drops the meta-pair's own subscription and whatever it currently points at.
void cancelMetaCallback(int meta_owner_id, std::string_view meta_pair_key);

void cancelMetaSubscription(int meta_owner_id, std::string_view meta_pair_key);

}  // namespace srnp

#endif
