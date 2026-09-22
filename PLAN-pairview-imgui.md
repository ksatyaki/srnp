# Plan: PairView on Dear ImGui, plus a documentation site

**Goal:** Bring the PairView blackboard inspector into the srnp repo as a Dear ImGui
application with full read/write control over the pair space, and write the
documentation srnp has never had — separate user and developer sites under MkDocs,
ready to deploy.

## Why PairView is being rewritten, not ported

The Qt version cannot compile against current srnp: it uses `initialize(argn, args, env)`,
`boost::posix_time`, `boost::bind` and `boost::mutex`, all of which are gone. It also
mutates Qt widgets directly from an srnp io thread, which Qt forbids, and holds a
`map_mutex` it never locks. Its own source carries the right idea as a TODO:
*"Redundant data. The pairspace can already give this."* Correct — the client and server
share one `PairSpace` in-process, so an immediate-mode GUI reads it each frame and keeps
no mirror at all. That removes the mirror, the cross-thread widget access and the unused
mutex in one move.

## Approach — the GUI

**Verdict on Dear ImGui here: adopt.** It fits a per-frame redraw from a shared data
structure, needs no code generation step (no `moc`, no `.ui`), and is one submodule plus
two backend files. Against it: no native widgets, no accessibility, and text input is
adequate rather than good — all acceptable for a developer monitoring tool. MIT, so
GPLv3-compatible; GLFW is zlib, likewise.

- **Platform layer:** GLFW as a system dependency (3.4 present here), OpenGL 3 renderer.
  Only `external/imgui` is submoduled.
- **Data flow:** no callbacks, no mirrored map. The render loop calls
  `srnp::snapshotPairs()` and `srnp::components()` (both new, added in M1) once per frame
  and draws from the copies. A standing wildcard subscription is what makes other
  components' pairs arrive in the local pair space at all.
- **Frame pacing:** `glfwWaitEventsTimeout(0.1)`. Redraws on input, otherwise ten times a
  second — ample for watching a blackboard, near-zero cost.
- **Startup:** the window opens first. `initialize()` blocks for up to `kReadyTimeout`
  (10 s), so it runs on a worker thread while the UI stays live and shows a disconnected
  banner with the address it tried. Failure retries every 3 s.

## Approach — the documentation

**One MkDocs site, two top-level sections.** User docs and developer docs are separate
navigation trees with separate landing pages, so a reader lands in one and never has to
wade through the other. They share a build because cross-linking between them has to work
and two sites would make that fragile.

**The API reference is hand-written, not generated.** Across the 13 public headers there
are 19 `/** */` blocks and everything else is a single `///` line, so Doxygen would emit a
class list of one-line briefs with undocumented parameters — and it would emit
`FrameChannel` and the session classes alongside the twenty functions a user actually
calls. A curated page covering the real public surface is better documentation. The usual
objection to hand-writing is drift, and that one is real, so it gets answered directly:
`tools/check_api_reference.py` fails if a function declared in `srnp_kernel.h` or
`meta_pair_callback.hpp` has no entry on the reference page.

**The drift check runs as a ctest test, not a CI job.** There are no docs jobs in CI by
decision, but the check costs nothing and the existing suite already runs everywhere. It
is registered behind `find_package(Python3)` and skipped when python3 is absent.

**Pages stay in portable Markdown.** Plain CommonMark, tables, and mermaid fences —
no Material-only syntax (admonitions, content tabs, annotations), which renders as raw
text for anyone reading these files on GitHub. Mermaid works both in Material and in
GitHub's renderer, so diagrams need no image files and no build step.

**The README becomes minimal and self-contained.** What srnp is, dependencies, build,
run, licence, and a link to the documentation site. Everything else moves into `docs/`.

## Documentation layout

```
mkdocs.yml
tools/check_api_reference.py
docs/
  requirements.txt         pinned mkdocs + mkdocs-material; Cloudflare installs from this
  index.md                 what srnp is, and which of the two sections you want
  user/
    index.md
    quickstart.md          master, env vars, a publisher and a subscriber, expected output
    concepts.md            owners, pairs, the pair space, subscriptions, callbacks,
                           meta-pairs, what the master does and does not do,
                           the PEIS-to-srnp function table moved out of the README
    building.md            dependencies, cmake, install, LD_LIBRARY_PATH
    running.md             srnp-master, SRNP_MASTER_IP/PORT, SRNP_LOG_LEVEL, log levels
    pairview.md            the GUI: what it shows, every control, screenshots  (M2)
    api-reference.md       curated reference for the public API
    changelog.md           the 0.2.0 section moved out of the README
  dev/
    index.md
    architecture.md        master and components, what lives where, mermaid diagrams
    threading.md           io_context, strands, the socket rules, every mutex
    protocol.md            byte-level wire format and every message flow
    internals.md           file map, FrameChannel, the session classes, PairQueue, codec
    testing.md             ctest, unit vs integration, sanitizers, the tsan suppressions
    gui.md                 GUI architecture and how to add a panel  (M2)
```

### What the harder pages must contain

- **`user/api-reference.md`** — grouped by task, not by header: lifecycle, writing,
  reading, subscriptions, callbacks, meta-pairs, types and constants. Each entry gives the
  signature, what it does, what the return value means, whether it is safe to call from
  any thread, and the gotcha where there is one. Two gotchas that must be written down
  because they are invisible from the signature: `registerCallback(owner, "*", fn)` returns
  `kInvalidCallbackHandle` and so can never be cancelled, and `registerSubscription`
  returns `kInvalidSubscriptionHandle` when you are already subscribed.
- **`user/concepts.md`** — must state plainly that `expiry_time_` is carried on the wire
  and read back but never acted on by anything. It is not a working feature and the docs
  should not imply it is.
- **`dev/threading.md`** — the rules that were learned the hard way and exist nowhere in
  writing: a socket is touched only on its own strand, `FrameChannel::read` runs on the
  strand while `send` and `close` may be called from anywhere, closes are posted rather
  than performed inline, and the acceptor has its own strand for the same reason.
- **`dev/protocol.md`** — the 16-byte header as an offset/size/field table, little-endian,
  `kMaxPayload`, the string encoding (`u32` length then bytes), times as `int64`
  nanoseconds since the epoch, then every `MessageType` with its payload layout and the
  four flows: a component joining, subscribing, a value update, and a removal.
- **`dev/internals.md`** — must explain why a `PairUpdate` frame carries an empty payload
  with the pair itself arriving on `PairQueue`. It is the most surprising thing in the
  codebase and reads like a bug until you know it is an in-process handoff.

---

## Milestone 1 — library additions, and the documentation site

Independently buildable and testable with no GUI present.

### Deleting a pair

There is no way to delete a pair today. The closest thing is a tombstone — setting a pair
to `Type::Invalid`, which is already filtered out of wildcard and subscription sends — but
that leaves the entry in the map forever and reads as a trick rather than an operation.
Two new message types instead, mirroring exactly how `Pair` and `PairUpdate` already pair
up:

- `RemovePair` — client to the owning component's server: "delete this pair".
- `PairRemoved` — the owner's server out to its subscribers: "this pair is gone".

Payload for both: `int32 owner`, length-prefixed `string key`. Both travel the route a
value update already travels, so no new plumbing is needed.

### Changes

- `include/srnp/wire.h` — add `RemovePair` and `PairRemoved` to `MessageType`.
- `src/wire.cpp` — extend the `decodeHeader` enum-range check to the new last value.
- `include/srnp/msgs/CommMessages.h`, `include/srnp/msgs/codec.h`, `src/codec.cpp` — a
  `RemovePairRequest { int owner; std::string key; }` message with `encode`/`decode`.
- `src/server.cpp` — handle `RemovePair` (erase from the pair space if we own the key,
  then notify subscribers); handle `PairRemoved` from a peer by erasing the local copy.
- `src/client.cpp`, `include/srnp/client.h` — `removePair(key)` and
  `removeRemotePair(owner, key)`; remember every known component in a
  `std::map<int, ComponentInfo>` guarded by `state_mutex_` and expose `components()`.
- `include/srnp/srnp_kernel.h`, `src/srnp_kernel.cpp` — free functions `removePair`,
  `removeRemotePair`, `components()`, and `snapshotPairs()` returning `std::vector<Pair>`
  copied under the pair-space lock.
- `mkdocs.yml`, `docs/**`, `tools/check_api_reference.py` — the site above.
- `tests/CMakeLists.txt` — register the API-reference check as a ctest test.
- `README.md` — cut down to the minimal form.

### Steps

- [ ] Add the two message types and extend the header validity check.
- [ ] Add `RemovePairRequest` with codec functions; round-trip and rejection tests in
      `tests/test_wire.cpp` alongside the existing ones.
- [ ] Implement removal in `ServerSession`: delete when we are the owner, forward
      `PairRemoved` to that pair's subscribers, and apply an inbound `PairRemoved`.
- [ ] Decide by reading `PairSpace::addSubscription` whether a removed pair's subscriber
      list must survive the deletion so a later re-publish still reaches the same
      subscribers. Implement whichever the read shows is correct and say which in the
      commit message.
- [ ] Add `Client::removePair` / `removeRemotePair`, and the `components()` map fed from
      `onMasterMessage`, `addSession` and `removeSession`.
- [ ] Add the four kernel free functions, including `snapshotPairs()`.
- [ ] Integration tests in `tests/test_integration.cpp`: a component deletes its own pair
      and a subscriber sees it vanish; a component deletes a remote pair and the owner
      drops it; `components()` reflects a node joining and leaving.
- [ ] Scaffold `mkdocs.yml` and `docs/requirements.txt` with pinned versions; confirm
      `mkdocs build --strict` is clean, which also catches every broken internal link.
- [ ] Write the user section: index, quickstart, concepts, building, running, changelog.
      Build and run every command and code sample in them; a quickstart that does not work
      verbatim is worse than none.
- [ ] Write the developer section: index, architecture, threading, protocol, internals,
      testing. Generate the protocol tables by reading `wire.h` and `codec.cpp`, not from
      memory.
- [ ] Write `user/api-reference.md` covering the public API including the M1 additions.
- [ ] Write `tools/check_api_reference.py` and register it as a ctest test guarded by
      `find_package(Python3 COMPONENTS Interpreter QUIET)`. It supports an explicit
      skip-list with a written reason per entry, so a deliberate omission is visible
      rather than silent.
- [ ] Cut `README.md` down: what srnp is, dependencies, build, run, documentation link,
      licence. The documentation site URL appears exactly once, as
      `https://SRNP_DOCS_URL_PLACEHOLDER`, with a line noting the same pages live in
      `docs/` in the meantime.
- [ ] Build clean with `-Wall -Wextra`, full `ctest` green, `mkdocs build --strict` clean.
- [ ] Commit.

---

## Milestone 2 — the GUI

### Changes

- `.gitmodules`, `external/imgui` — submodule `github.com/ocornut/imgui` pinned to a
  release tag on the master branch (not docking; multi-viewport buys nothing here).
- `CMakeLists.txt` — `option(SRNP_BUILD_GUI "Build the PairView GUI" ON)`; when the
  submodule is absent or GLFW/OpenGL are not found, print a status line and skip the
  target rather than failing the build.
- `gui/CMakeLists.txt` — an `imgui` static target from the five core sources plus
  `backends/imgui_impl_glfw.cpp` and `backends/imgui_impl_opengl3.cpp`. It deliberately
  does **not** link `srnp_warnings`: vendored code under `-Wall -Wextra` is noise.
- `gui/main.cpp` — argument handling, GLFW/ImGui setup, the render loop, teardown.
- `gui/srnp_link.h/.cpp` — connection state machine on a `std::jthread`: tries
  `initialize`, records `Disconnected`/`Connecting`/`Connected` plus the last error,
  retries every 3 s, registers the wildcard subscription on success.
- `gui/pair_view.h/.cpp` — all drawing, and the UI's own state (selection, filter text,
  per-pair subscription handles).
- `gui/format.h/.cpp` — the parts with no ImGui in them: escaping a value for display,
  hex-dumping it, formatting an age, parsing `(META <owner> <key>)`, filter matching.
- `tests/test_gui_format.cpp` — unit tests for `gui/format`.
- `docs/user/pairview.md`, `docs/dev/gui.md`, `mkdocs.yml` nav — the GUI's pages.
- `README.md` — one line under Running mentioning the GUI, linking to its page.

### Window layout

One window, three regions.

**Left — the tree.** Grouped by owner, an `ImGuiTreeNode` per component, one row per pair.
Columns: key, value (escaped, truncated), age. The component header shows the owner id,
its `ip:port` from `components()`, "(you)" for our own id, and "(gone)" greyed out when the
master no longer lists it but its pairs are still in our space. Above it: a filter box
matching on key substring, and an owner filter.

**Right top — detail for the selected pair.** Owner, key, type, write time and age, expiry
("not set" when epoch), the full escaped value, and a hex dump for `Type::Bytes`. When the
value parses as `(META <owner> <key>)`, the resolved target is shown with a button that
selects it in the tree.

**Right bottom — controls.**
- Post: owner / key / value fields plus type. Owner equal to ours or left blank goes to
  `setPair`, anything else to `setRemotePair`.
- Delete: acts on the selected pair, behind a confirmation popup, since it is
  irreversible.
- Subscriptions: a "Subscribe to everything (\*)" checkbox, on at startup. While it is on,
  per-pair subscription toggles are disabled and say why — a wildcard subscription already
  covers every key, so a per-key toggle under it would do nothing. Turning it off cancels
  the wildcard and enables per-row toggles backed by `registerSubscription(owner, key)` /
  `cancelSubscription(handle)`.

### Steps

- [ ] Add the imgui submodule at a pinned tag; add `gui/CMakeLists.txt` with the `imgui`
      target and the `SRNP_BUILD_GUI` option and skip logic.
- [ ] `gui/format` plus `tests/test_gui_format.cpp`: escaping (including embedded nulls
      and high bytes), hex dump, age formatting, meta parsing, filter matching.
- [ ] `gui/srnp_link`: worker-thread connect with retry, state and last-error readable
      from the render thread, wildcard subscription on connect, worker joined before
      `shutdown()`.
- [ ] `gui/main.cpp`: GLFW window, ImGui context, `glfwWaitEventsTimeout(0.1)` loop,
      clean teardown.
- [ ] Draw the tree with filtering, owner grouping and the gone/you markers.
- [ ] Draw the detail pane including the hex dump and meta-pair navigation.
- [ ] Post, delete-with-confirmation, and the subscription controls.
- [ ] Disconnected banner showing the attempted address and the error.
- [ ] Run the master plus two demo nodes and check by hand: pairs appear, update, and
      disappear on delete; a killed node is marked gone; posting from the GUI reaches
      another node.
- [ ] Write `docs/user/pairview.md` with screenshots taken from that session, and
      `docs/dev/gui.md`; add both to the nav; add the README line.
- [ ] `mkdocs build --strict` clean, `ctest` green.
- [ ] Commit.

---

## After the plan — deploying the site

Not a milestone and not part of the two commits. Done on request once the above is
implemented, as its own change:

- Create a Cloudflare Pages project against this repo. Build command
  `pip install -r docs/requirements.txt && mkdocs build`, output directory `site`.
- Set the real `site_url` in `mkdocs.yml`.
- Replace `https://SRNP_DOCS_URL_PLACEHOLDER` in `README.md` with the deployed URL and
  drop the "the same pages live in `docs/`" line.
- Decide then whether a GitHub Actions job should build the docs on push as a check.

Until that runs, the README's documentation link does not resolve. That is why the URL
appears in exactly one place and why the README says where the pages are in the meantime.

## Protocol

Two commits, one per milestone. Not one per file or per step.

## Edge cases & risks

- **Values are not text.** `Type::Bytes` can hold embedded nulls and arbitrary bytes.
  Every value reaching ImGui goes through the escaper in `gui/format`; nothing is passed
  raw to a `%s`.
- **A deleted pair that is still subscribed.** If a component republishes a key that was
  deleted, subscribers must still receive it. The M1 step above settles this by reading
  `PairSpace::addSubscription` rather than guessing.
- **Wildcard subscription and per-pair toggles overlap.** Handled by disabling the
  per-pair toggles while the wildcard is on, with the reason shown in the UI.
- **Documentation drifting from the code.** The API reference is covered by the ctest
  check. The protocol and threading pages are not, and are the pages most likely to go
  stale — both say at the top which files they describe, so a reader who suspects drift
  knows where to look.
- **Protocol compatibility.** The new message types change what a peer must understand.
  Nothing outside this repo speaks the protocol — the old PairView was the only other
  consumer and it is being replaced here — so no version negotiation is added. Mixed old
  and new binaries will log an unknown-message-type warning and drop the frame, which is
  the existing behaviour for anything unrecognised.
- **Build machines without GLFW.** The GUI target is skipped with a status message, not an
  error, so `cmake` still succeeds for a library-only build.
- **Sanitizer jobs.** The GUI is built in the plain CI job only. Running a GL driver under
  TSan produces noise unrelated to srnp.
