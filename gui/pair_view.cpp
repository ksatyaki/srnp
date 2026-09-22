/*
  pair_view.cpp - Implementation of the window.

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

#include "pair_view.h"

#include "format.h"
#include "theme.h"

#include <imgui.h>

#include <algorithm>
#include <charconv>
#include <format>

namespace srnp::gui {
namespace {

/// How much of a value fits in the tree's value column.
constexpr std::size_t kValueColumnChars = 48;

constexpr ImVec4 kGrey{0.55f, 0.55f, 0.55f, 1.0f};
constexpr ImVec4 kRed{0.90f, 0.40f, 0.40f, 1.0f};
constexpr ImVec4 kGreen{0.40f, 0.80f, 0.45f, 1.0f};

/// The types a user may publish. Invalid is not one of them.
constexpr std::array<std::pair<const char*, Pair::Type>, 3> kPostTypes{{
    {"String", Pair::Type::String},
    {"Bytes", Pair::Type::Bytes},
    {"Meta", Pair::Type::Meta},
}};

/// An ImGui input backed by a std::string, without a fixed-size buffer.
/// The label is hidden; callers draw their own to the left of the field.
bool inputText(const char* label, std::string& text, const char* hint,
               ImGuiInputTextFlags flags = 0) {
  return ImGui::InputTextWithHint(
      label, hint, text.data(), text.capacity() + 1,
      flags | ImGuiInputTextFlags_CallbackResize,
      [](ImGuiInputTextCallbackData* data) {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
        auto* backing = static_cast<std::string*>(data->UserData);
        backing->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = backing->data();
        return 0;
      },
      &text);
}

std::optional<int> parseOwner(std::string_view text) {
  int value = 0;
  const auto* end = text.data() + text.size();
  if (std::from_chars(text.data(), end, value).ptr != end) return std::nullopt;
  return value;
}

void textColoured(const ImVec4& colour, std::string_view text) {
  ImGui::TextColored(colour, "%.*s", static_cast<int>(text.size()), text.data());
}

void text(std::string_view value) {
  ImGui::TextUnformatted(value.data(), value.data() + value.size());
}

/// Wrapped to the width available, so a long line never runs off the pane.
void wrapped(std::string_view value) {
  ImGui::TextWrapped("%.*s", static_cast<int>(value.size()), value.data());
}

void wrappedColoured(const ImVec4& colour, std::string_view value) {
  ImGui::PushStyleColor(ImGuiCol_Text, colour);
  wrapped(value);
  ImGui::PopStyleColor();
}

/// An icon and a caption, followed by the control on the same line.
void fieldLabel(const char* icon, const char* caption) {
  text(std::format("{}  {}", icon, caption));
  ImGui::SameLine();
}

/// A section heading: the icon, then the label, in the heading face.
void heading(ImFont* font, const char* icon, const char* label) {
  if (font != nullptr) ImGui::PushFont(font);
  ImGui::SeparatorText(std::format("{}  {}", icon, label).c_str());
  if (font != nullptr) ImGui::PopFont();
}

}  // namespace

void PairView::draw() {
  const auto connected = link_.state() == SrnpLink::State::Connected;

  const auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("PairView", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  if (!connected) {
    drawDisconnectedBanner();
    ImGui::End();
    return;
  }

  // One read per frame, drawn from the copies. Calling snapshotPairs() again
  // further down would risk drawing two different versions of the same frame.
  const auto pairs = snapshotPairs();
  const auto components = srnp::components();

  if (ImGui::BeginTable("layout", 2, ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("tree", ImGuiTableColumnFlags_WidthStretch, 0.55f);
    ImGui::TableSetupColumn("side", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    drawTree(pairs, components);

    ImGui::TableNextColumn();
    drawDetail(pairs);
    ImGui::Separator();
    drawControls(pairs);

    ImGui::EndTable();
  }

  ImGui::End();
}

void PairView::drawDisconnectedBanner() {
  const auto connecting = link_.state() == SrnpLink::State::Connecting;

  if (fonts_.heading != nullptr) ImGui::PushFont(fonts_.heading);
  textColoured(connecting ? kGrey : kRed,
               std::format("{}  {}", connecting ? icon::kRotate : icon::kPlug,
                           connecting ? "Connecting..." : "Not connected to a master"));
  if (fonts_.heading != nullptr) ImGui::PopFont();
  ImGui::Separator();

  text(std::format("{}  Master: {}", icon::kServer, link_.address()));

  if (const auto error = link_.lastError(); !error.empty()) {
    wrapped(std::format("{}  Last error: {}", icon::kWarning, briefError(error)));
    ImGui::Spacing();
    wrapped("Retrying every 3 seconds. Check that srnp-master is running, and that "
            "SRNP_MASTER_IP and SRNP_MASTER_PORT point at it.");
  }
}

void PairView::drawTree(const std::vector<Pair>& pairs,
                        const std::map<int, ComponentInfo>& components) {
  fieldLabel(icon::kFilter, "Keys");
  ImGui::SetNextItemWidth(220);
  inputText("##key filter", key_filter_, "any key");
  tooltip("Show only keys containing this text. Case-insensitive.");
  ImGui::SameLine();

  // The owner filter offers every owner on screen, plus "all".
  std::vector<int> owners;
  for (const auto& pair : pairs)
    if (std::ranges::find(owners, pair.getOwner()) == owners.end())
      owners.push_back(pair.getOwner());
  std::ranges::sort(owners);

  const auto owner_label =
      owner_filter_ == kAnyOwner ? std::string("all owners") : std::to_string(owner_filter_);
  fieldLabel(icon::kUsers, "Owner");
  ImGui::SetNextItemWidth(170);
  if (ImGui::BeginCombo("##owner filter", owner_label.c_str())) {
    if (ImGui::Selectable("all owners", owner_filter_ == kAnyOwner)) owner_filter_ = kAnyOwner;
    for (const int owner : owners)
      if (ImGui::Selectable(std::to_string(owner).c_str(), owner_filter_ == owner))
        owner_filter_ = owner;
    ImGui::EndCombo();
  }

  std::map<int, std::vector<Row>> by_owner;
  const int us = getOwnerID();
  for (const auto& pair : pairs) {
    if (owner_filter_ != kAnyOwner && pair.getOwner() != owner_filter_) continue;
    if (!matchesFilter(pair.getKey(), key_filter_)) continue;
    by_owner[pair.getOwner()].push_back(Row{&pair, pair.getOwner() == us});
  }

  ImGui::Separator();
  ImGui::BeginChild("tree", ImVec2(0, 0));

  if (by_owner.empty()) textColoured(kGrey, pairs.empty() ? "No pairs yet." : "Nothing matches.");

  for (const auto& [owner, rows] : by_owner) drawOwner(owner, rows, components);

  ImGui::EndChild();
}

void PairView::drawOwner(int owner, const std::vector<Row>& rows,
                         const std::map<int, ComponentInfo>& components) {
  const bool ours = owner == getOwnerID();
  const auto component = components.find(owner);

  std::string header = std::format("{}  Owner {}", icon::kServer, owner);
  if (ours)
    header += "  (you)";
  else if (component != components.end())
    header += std::format("   {}:{}", component->second.ip, component->second.port);

  if (owner_to_open_ == owner) {
    ImGui::SetNextItemOpen(true);
    owner_to_open_.reset();
  }

  ImGui::PushID(owner);
  if (fonts_.heading != nullptr) ImGui::PushFont(fonts_.heading);
  const bool open = ImGui::TreeNodeEx("owner", ImGuiTreeNodeFlags_DefaultOpen, "%s",
                                      header.c_str());
  if (fonts_.heading != nullptr) ImGui::PopFont();

  // The master no longer lists this component, but we still hold its pairs.
  if (!ours && component == components.end()) {
    ImGui::SameLine();
    textColoured(kGrey, "(gone)");
  }

  if (open) {
    if (ImGui::BeginTable("pairs", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.35f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
      ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthStretch, 0.20f);
      ImGui::TableHeadersRow();

      for (const auto& row : rows) {
        const Pair& pair = *row.pair;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const bool chosen = selection_ && selection_->owner == owner &&
                            selection_->key == pair.getKey();
        ImGui::PushID(pair.getKey().c_str());
        if (ImGui::Selectable(pair.getKey().c_str(), chosen,
                              ImGuiSelectableFlags_SpanAllColumns))
          select(owner, pair.getKey());
        ImGui::PopID();

        ImGui::TableNextColumn();
        text(escapeTruncated(pair.getValue(), kValueColumnChars));

        ImGui::TableNextColumn();
        text(formatAge(Clock::now() - pair.getWriteTime()));
      }
      ImGui::EndTable();
    }
    ImGui::TreePop();
  }
  ImGui::PopID();
}

const Pair* PairView::selected(const std::vector<Pair>& pairs) const {
  if (!selection_) return nullptr;

  const auto it = std::ranges::find_if(pairs, [this](const Pair& pair) {
    return pair.getOwner() == selection_->owner && pair.getKey() == selection_->key;
  });
  return it == pairs.end() ? nullptr : &*it;
}

void PairView::select(int owner, std::string key) {
  selection_ = PairKey{owner, std::move(key)};
}

void PairView::drawDetail(const std::vector<Pair>& pairs) {
  heading(fonts_.heading, icon::kCube, "Selected pair");

  const Pair* pair = selected(pairs);
  if (pair == nullptr) {
    textColoured(kGrey, selection_ ? "The selected pair is gone." : "Nothing selected.");
    return;
  }

  ImGui::BeginChild("detail", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.5f));

  text(std::format("Owner    {}", pair->getOwner()));
  text(std::format("Key      {}", pair->getKey()));
  text(std::format("Type     {}", nameOfType(pair->getType())));
  text(std::format("Written  {}  ({})", formatTime(pair->getWriteTime()),
                   formatAge(Clock::now() - pair->getWriteTime())));
  // The field is on the wire and nothing acts on it. Shown, and labelled.
  text(std::format("Expiry   {}  (never enforced)", formatTime(pair->getExpiryTime())));

  if (const auto target = parseMetaTarget(pair->getValue())) {
    ImGui::Spacing();
    text(std::format("{}  Points at [{}] {}", icon::kLink, target->owner, target->key));
    ImGui::SameLine();
    if (ImGui::SmallButton(icon::kArrowRight)) {
      select(target->owner, target->key);
      owner_to_open_ = target->owner;
    }
    tooltip("Select the pair this one points at");
  }

  ImGui::Spacing();
  heading(fonts_.heading, icon::kTag, "Value");
  ImGui::TextWrapped("%s", escape(pair->getValue()).c_str());

  if (pair->getType() == Pair::Type::Bytes) {
    ImGui::Spacing();
    heading(fonts_.heading, icon::kCube,
            std::format("Hex ({} bytes)", pair->getValue().size()).c_str());
    text(hexDump(pair->getValue()));
  }

  ImGui::EndChild();
}

void PairView::drawControls(const std::vector<Pair>& pairs) {
  drawPostForm();
  ImGui::Spacing();
  drawDeleteButton(pairs);
  ImGui::Spacing();
  drawSubscriptionControls();

  if (!status_.empty()) {
    ImGui::Spacing();
    const bool failed = status_.starts_with("Could not");
    wrappedColoured(failed ? kRed : kGreen,
                    std::format("{}  {}", failed ? icon::kCircleX : icon::kCircleCheck,
                                status_));
  }
}

void PairView::drawPostForm() {
  heading(fonts_.heading, icon::kSend, "Post a pair");

  fieldLabel(icon::kUsers, "Owner");
  ImGui::SetNextItemWidth(150);
  inputText("##post owner", post_owner_, "you");
  tooltip("Leave blank to publish under your own id. Another id is sent to\n"
          "that component, which then owns the pair.");

  fieldLabel(icon::kTag, "Key  ");
  ImGui::SetNextItemWidth(-1);
  inputText("##post key", post_key_, "required");

  fieldLabel(icon::kCube, "Value");
  ImGui::SetNextItemWidth(-1);
  inputText("##post value", post_value_, "may be empty");

  fieldLabel(icon::kInfo, "Type ");
  ImGui::SetNextItemWidth(150);
  if (ImGui::BeginCombo("##post type", kPostTypes[post_type_].first)) {
    for (int i = 0; i < static_cast<int>(kPostTypes.size()); ++i)
      if (ImGui::Selectable(kPostTypes[i].first, post_type_ == i)) post_type_ = i;
    ImGui::EndCombo();
  }

  const bool ready = !post_key_.empty();
  ImGui::BeginDisabled(!ready);
  if (ImGui::Button(icon::kSend)) {
    const auto type = kPostTypes[post_type_].second;
    const auto owner = post_owner_.empty() ? getOwnerID() : parseOwner(post_owner_).value_or(-2);

    if (owner == -2) {
      status_ = std::format("Could not post: \"{}\" is not an owner id", post_owner_);
    } else if (owner == getOwnerID()) {
      status_ = setPair(post_key_, post_value_, type)
                    ? std::format("Posted {}", post_key_)
                    : std::format("Could not post {}", post_key_);
    } else {
      status_ = setRemotePair(owner, post_key_, post_value_, type)
                    ? std::format("Posted {} to owner {}", post_key_, owner)
                    : std::format("Could not post to owner {}: not connected", owner);
    }
  }
  ImGui::EndDisabled();
  tooltip("Post the pair. An owner other than yours is sent to that component.");
  if (!ready) {
    ImGui::SameLine();
    textColoured(kGrey, "a key is required");
  }
}

void PairView::drawDeleteButton(const std::vector<Pair>& pairs) {
  heading(fonts_.heading, icon::kTrash, "Delete");

  const Pair* pair = selected(pairs);
  ImGui::BeginDisabled(pair == nullptr);
  if (ImGui::Button(icon::kTrash)) ImGui::OpenPopup("Confirm delete");
  ImGui::EndDisabled();
  tooltip("Delete the selected pair. Asks first; it cannot be undone.");

  if (pair == nullptr) {
    ImGui::SameLine();
    textColoured(kGrey, "select a pair first");
    return;
  }

  // A deletion cannot be undone, so it is worth one extra click.
  if (ImGui::BeginPopupModal("Confirm delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (fonts_.heading != nullptr) ImGui::PushFont(fonts_.heading);
    text(std::format("{}  Delete [{}] {}?", icon::kWarning, pair->getOwner(),
                     pair->getKey()));
    if (fonts_.heading != nullptr) ImGui::PopFont();
    textColoured(kGrey, "This cannot be undone.");
    ImGui::Spacing();

    if (ImGui::Button(std::format("{}  Delete", icon::kTrash).c_str())) {
      const auto owner = pair->getOwner();
      const auto key = pair->getKey();
      const bool sent = owner == getOwnerID() ? removePair(key) : removeRemotePair(owner, key);

      status_ = sent ? std::format("Deleted [{}] {}", owner, key)
                     : std::format("Could not delete [{}] {}: not connected", owner, key);
      selection_.reset();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(std::format("{}  Cancel", icon::kCircleX).c_str()))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void PairView::drawSubscriptionControls() {
  heading(fonts_.heading, icon::kEye, "Subscriptions");

  bool everything = link_.subscribedToAll();
  if (ImGui::Checkbox("Subscribe to everything (*)", &everything)) {
    link_.setSubscribedToAll(everything);
    if (!everything) {
      // The wildcard is gone; anything we subscribed to per pair is not.
      status_ = "Wildcard subscription cancelled. Per-pair toggles are now live.";
    }
  }

  tooltip("A standing wildcard subscription. Without it, no other component's\n"
          "pairs ever reach this node's pair space.");

  if (everything) {
    wrappedColoured(kGrey,
                    "A wildcard subscription already covers every key, so the per-pair "
                    "toggles below would do nothing. Turn it off to use them.");
  }

  ImGui::BeginDisabled(everything);
  for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
    ImGui::PushID(it->first.key.c_str());
    bool on = true;
    if (ImGui::Checkbox(std::format("[{}] {}", it->first.owner, it->first.key).c_str(), &on)) {
      cancelSubscription(it->second);
      it = subscriptions_.erase(it);
      ImGui::PopID();
      continue;
    }
    ImGui::PopID();
    ++it;
  }
  ImGui::EndDisabled();

  if (everything || !selection_) return;

  if (!subscriptions_.contains(*selection_) &&
      ImGui::Button(std::format("{}  Subscribe to the selected pair", icon::kEye).c_str())) {
    const auto handle = registerSubscription(selection_->owner, selection_->key);
    if (handle == kInvalidSubscriptionHandle)
      status_ = std::format("Already subscribed to [{}] {}", selection_->owner,
                            selection_->key);
    else
      subscriptions_.emplace(*selection_, handle);
  }
}

}  // namespace srnp::gui
