/*
  pair_view.h - The window: the tree, the detail pane, and the controls.

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

#ifndef SRNP_GUI_PAIR_VIEW_H_
#define SRNP_GUI_PAIR_VIEW_H_

#include "srnp_link.h"
#include "theme.h"

#include <srnp/Pair.h>
#include <srnp/msgs/MasterMessages.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace srnp::gui {

/**
 * Draws everything, and holds the state that belongs to the UI rather than
 * to srnp: what is selected, what is typed into the filter and the post
 * form, and which pairs have their own subscription.
 *
 * It keeps no copy of the pair space. Each frame reads snapshotPairs() and
 * components() and draws from those, so what is on screen is what srnp
 * holds and there is nothing to keep in step.
 */
class PairView {
 public:
  PairView(SrnpLink& link, Fonts fonts) : link_(link), fonts_(fonts) {}

  /// One frame.
  void draw();

  /// Selects a pair, as clicking its row does. Public because the meta-pair
  /// jump and the tests both need it.
  void select(int owner, std::string key);

 private:
  /// A pair and where it came from, as this frame sees it.
  struct Row {
    const Pair* pair = nullptr;
    bool ours = false;
  };

  void drawDisconnectedBanner();
  void drawTree(const std::vector<Pair>& pairs,
                const std::map<int, ComponentInfo>& components);
  void drawOwner(int owner, const std::vector<Row>& rows,
                 const std::map<int, ComponentInfo>& components);
  void drawDetail(const std::vector<Pair>& pairs);
  void drawControls(const std::vector<Pair>& pairs);
  void drawPostForm();
  void drawDeleteButton(const std::vector<Pair>& pairs);
  void drawSubscriptionControls();

  /// The selected pair out of this frame's snapshot, if it is still there.
  const Pair* selected(const std::vector<Pair>& pairs) const;

  SrnpLink& link_;
  Fonts fonts_;

  std::optional<PairKey> selection_;
  /// Set when the meta-pair jump button picks a new selection, so the tree
  /// opens the owner it is under.
  std::optional<int> owner_to_open_;

  std::string key_filter_;
  int owner_filter_ = kAnyOwner;

  /// The post form's fields. Kept as text so they survive a frame where the
  /// pair space does not contain what is being typed yet.
  std::string post_owner_;
  std::string post_key_;
  std::string post_value_;
  int post_type_ = 0;  // Indexes kPostTypes in the .cpp; 0 is String.

  std::string status_;

  /// Per-pair subscriptions this window made, so they can be cancelled.
  std::map<PairKey, SubscriptionHandle> subscriptions_;
};

}  // namespace srnp::gui

#endif  // SRNP_GUI_PAIR_VIEW_H_
