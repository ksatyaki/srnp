# Plan: SRNP modernization

**Goal:** Make SRNP build and run correctly on a current toolchain (Boost 1.90 / C++20), eliminate the undefined behaviour and data races found in review, and replace the Boost-era idioms and text wire format with modern C++ — verified by a test suite that does not exist today.

## Approach

Two milestones with a hard boundary between them.

**M1 makes the repo verifiable.** It ports the code far enough to compile, turns on warnings and sanitizers, deletes dead subsystems, fixes every self-contained correctness bug, and builds the test harness. Nothing structural changes. At the end of M1 the repo builds warning-clean, runs green under ASan/UBSan/TSan, and has tests that will catch M2 regressions.

**M2 does the structural work** against that safety net: C++20 coroutines, a binary wire format, the `send<T>()` template, the `PairSpace` container swap, and the Boost→std library swap.

Decisions taken during planning:

- **Binary wire format**, hand-rolled. Breaks wire compatibility; nothing in-tree depends on the format, and PairView links the library rather than speaking the protocol, so it needs a rebuild, not a rewrite.
- **C++20 coroutines** for the async rewrite — but the `delete this` lifetime fix lands in M1 via `enable_shared_from_this`, so the coroutine port in M2 carries only control-flow changes, not lifetime changes.
- **Keep the client↔own-server loopback.** Removing `PairQueue` / `PAIR_NOCOPY` is an architecture change, not a cleanup, and it would split one uniform code path into two.
- **Stay on Boost.Asio.** Standalone Asio would drop the last Boost dependency but isn't reliably distro-packaged; that trades a packaged dependency for a vendored one.
- **Delete the Python wrapper and catkin support.** Boost.Python against Python 2.7, self-described as "HIGHLY INCOMPLETE" and already commented out of the build; catkin is ROS 1 and end-of-life.

## Changes

### M1

- `CMakeLists.txt` — C++20, `Boost REQUIRED`, `target_include_directories`, `PUBLIC`/`PRIVATE` link scoping, `-Wall -Wextra`, opt-in sanitizer flags, `enable_testing()`. Drop the hardcoded `/usr/include/python2.7` and the `package.xml` install block.
- `CMakeLists_catkin.txt`, `package.xml`, `src/srnp_python.cpp` — deleted.
- `include/srnp/srnp_kernel.h`, `src/srnp_kernel.cpp` — drop `initialize_py`; `initialize` reports failure by return value or exception instead of `exit(0)`; `shutdown` stops and joins instead of `exit(0)`.
- `include/srnp/server.h`, `src/server.cpp` — `io_context`, `strand<io_context::executor_type>`, `std::array`; `ServerSession` becomes `shared_ptr` + `enable_shared_from_this`, every `delete this` removed; `TCP_NODELAY`; three writes coalesced into one scatter-gather write; `u_callback_` read under the pair-space lock.
- `include/srnp/client.h`, `src/client.cpp` — same Asio port; `ready_` becomes `std::atomic<bool>` set before the session is constructed; `sessions_map_` and `owner_id_` guarded by a mutex; the `it--` retry loop in `setPairUpdate` replaced; `delete[]` in `extractStrings`.
- `include/srnp/master_hub.h`, `src/master_hub.cpp` — `sendAsyncMsg` gains a per-session outbox so buffers outlive the write and messages stop interleaving; the blocking handshake reads move out of the accept handler; `main()` takes the listen port from argv/env.
- `src/PairSpace.cpp` — `isEnd` returns its comparison; `removeCallback` checks for `end()`.
- `include/srnp/Pair.h`, `include/srnp/msgs/MessageHeader.h`, `include/srnp/meta_pair_callback.hpp` — uninitialized members (`pair_type_`, `subscriber__`, the four `MetaCallbackInfo` handles) given initializers.
- `src/meta_pair_callback.cpp` — `meta_map` lookups stop using `operator[]` followed by `->`.
- `tests/` — new: unit tests plus a multi-process integration fixture.

### M2

- `include/srnp/msgs/` — new binary codec replacing every `boost::serialization` `serialize()` member.
- `src/client.cpp`, `src/server.cpp` — ten copies of the header/size/archive boilerplate collapse into one `send<T>()`; handler chains become coroutines.
- `include/srnp/PairSpace.h`, `src/PairSpace.cpp` — `std::vector<Pair>` → keyed map; `isEnd` and the iterator-returning lookup deleted.
- `include/srnp/Pair.h` — `CallbackHandle` becomes `uint64_t`; `callback_mutex` deleted; copy operations defaulted; `enum class PairType`.
- `include/srnp/client.h` — the four parallel subscription maps become one handle-keyed registry.
- Library-wide — `boost::shared_ptr`/`function`/`bind`/`thread`/`mutex`/`array`/`random`/`date_time`/`log` → standard-library equivalents.

## Steps

Checkboxes track progress; they are **not** commit boundaries. **One commit per milestone, two commits total.**

### Milestone 1 — build, verify, de-UB

**Clear the decks**

- [ ] Delete `src/srnp_python.cpp`, `CMakeLists_catkin.txt`, `package.xml`. Remove `initialize_py` from `srnp_kernel.h`/`.cpp` and the `package.xml` install block from `CMakeLists.txt`.
- [ ] Rewrite `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.16)`, `CMAKE_CXX_STANDARD 20`, `find_package(Boost REQUIRED COMPONENTS system thread serialization date_time)`, `target_include_directories(srnp PUBLIC ...)`, scoped `target_link_libraries`, `-Wall -Wextra`, an `SRNP_SANITIZE` option wiring `-fsanitize=address,undefined` / `-fsanitize=thread`, and `enable_testing()`. Drop the `/usr/include/python2.7` include path.

**Make it compile**

- [ ] Port Asio: `boost::asio::io_service` → `io_context`, `boost::asio::strand` → `boost::asio::strand<boost::asio::io_context::executor_type>` in `master_hub.h`, `boost::array` → `std::array` (with `<array>` included explicitly), and `<boost/bind/bind.hpp>` + `using namespace boost::placeholders` where `_1` is used.
- [ ] Build clean at `-Wall -Wextra`. Fix the five constructor member-initializer-order mismatches `-Wreorder` reports (`Client`, `ClientSession`, `ServerSession`, `MasterLink`, `MetaCallbackInfo`).

**Fix the self-contained bugs**

- [ ] `PairSpace::isEnd` — `return iter_to_check == pairs_.end();` (the function is deleted outright in M2; this is the minimal correct fix so tests can run).
- [ ] `PairSpace::removeCallback` — return early when `getPairIteratorWithOwnerAndKey` yields `end()`.
- [ ] `Client::setPairUpdate` — delete the `it--; usleep(100000); continue;` retry. Log the missing session and move to the next subscriber.
- [ ] `MasterHubSession::sendAsyncMsg` — give the session a `std::deque<std::string>` outbox: enqueue, and if no write is in flight start one; chain the next from the completion handler. Fixes both the use-after-free and the interleaved size/payload writes.
- [ ] `extractStrings` — `delete[] copyOfString`.
- [ ] Initialize `MessageHeader::subscriber__` in both constructors, `Pair::pair_type_` in the default constructor, and all four `MetaCallbackInfo` handle members.
- [ ] `meta_pair_callback.cpp` — replace `meta_map[key]->` with a `find()` + explicit miss check at every site, including the `else` branch that currently inserts a null `shared_ptr` and dereferences it.

**Fix lifetimes and races**

- [ ] `ServerSession` → `std::shared_ptr` + `enable_shared_from_this`. Remove every `delete this`; handlers capture `shared_from_this()`. This retires the double-free between the error paths and `Server::my_client_session_`.
- [ ] `Client::ready_` → `std::atomic<bool>`, initialized before `my_server_session_` is constructed. Replace the `while(!ready()) usleep(100)` spin in `initialize()` with `ready_.wait(false)`.
- [ ] Guard `Client::sessions_map_` and `Client::owner_id_` with a mutex covering both the io-thread writers (MM/UC handlers) and the user-thread readers (`setRemotePair`, `registerSubscription`, `cancelSubscription`).
- [ ] Read `PairSpace::u_callback_` under `pair_space_.mutex` in all three server handlers.
- [ ] Add a mutex around the `all_metas` / `meta_map` globals in `meta_pair_callback.cpp`.

**Fix shutdown and framing safety**

- [ ] `initialize()` — return an error or throw instead of `exit(0)` on missing env vars and on a malformed `--owner-id`. `shutdown()` — stop the `io_context`, join the spin threads, and return instead of `exit(0)`.
- [ ] Reject frames whose declared length exceeds a compile-time cap (16 MiB) before `resize()`, in both `ClientSession::handleMMandUCandPairMsgs` and `ServerSession::handleReadHeaderSize`.
- [ ] Set `tcp::no_delay(true)` on every socket, and coalesce the three sequential `boost::asio::write` calls into one scatter-gather write in `sendDataToServer`, `sendDataToClient`, `sendMasterMsgToOurClient` and `sendUpdateComponentsMsgToOurClient`. Record a before/after ping-pong latency number — this is the baseline M2 is measured against.

**Build the safety net**

- [ ] Add GoogleTest via `FetchContent` (BSD-3, GPLv3-compatible).
- [ ] `srnp-master` `main()` — take the listen port from `argv[1]` or `SRNP_MASTER_PORT`, defaulting to 12321, so tests can bind an ephemeral port.
- [ ] Unit tests: `PairSpace` add/update/remove, subscription add/remove including the universal path, callback registration and removal, and `CallbackHandle` uniqueness across many registrations. Add a test asserting `getPair` on an absent key returns empty — the regression test for `isEnd`.
- [ ] Unit tests: meta-pair value encode/parse round-trip through `extractStrings`, including the malformed inputs the current code warns about.
- [ ] Integration fixture: launch a master on an ephemeral port, launch two nodes with `--owner-id`, and assert (a) a pair set on A arrives at a subscriber on B, (b) a registered callback fires with the right value, (c) cancelling the subscription stops delivery, (d) a node disconnecting removes its subscriptions. Drive it on condition variables with a timeout, not `sleep`.
- [ ] Port `src/single_tests/*` onto the fixture or delete them; do not leave two parallel notions of "test" in the tree.
- [ ] Run the whole suite under ASan/UBSan and under TSan. Add a TSan suppressions file if Asio's internal synchronization produces known false positives; suppress by frame, never by disabling the run.
- [ ] Add a GitHub Actions workflow building both sanitizer configurations and running the suite.

### Milestone 2 — structural modernization

**Mechanical library swap**

- [ ] `boost::shared_ptr`/`shared_array` → `std::shared_ptr` / `std::make_shared` / `std::shared_ptr<T[]>`; `boost::function` → `std::function`; `boost::bind` + placeholders → lambdas; `boost::thread` → `std::jthread` with `stop_token`; `boost::mutex`/`scoped_lock` → `std::mutex`/`std::scoped_lock`; `boost::random` → `<random>`.
- [ ] `boost::posix_time::ptime` → `std::chrono::system_clock::time_point` throughout `Pair`; serialize as int64 nanoseconds since epoch.
- [ ] Replace `boost::log` and the `SRNP_PRINT_*` macros with a small `std::format`-based logger carrying `std::source_location`. Drop `WITH_BOOST_LOG` and its two code paths.
- [ ] Replace every `sprintf` / `ostringstream` construction (`setMetaPair`, `indicatePresence`, the port formatting in `initialize`) with `std::format`.

**Binary wire format**

- [ ] Define the frame: 16-byte little-endian header — u32 magic `SRNP`, u8 version, u8 type, u16 flags, u32 payload length, u32 subscriber (meaningful only for `PAIR_UPDATE_2`). One fixed-size read, validate magic/version/length-cap, one payload read.
- [ ] Write `encode`/`decode` free functions per message type over a `std::span<std::byte>` reader/writer: u32-length-prefixed strings, i32 owner ids, i64 nanosecond timestamps, u8 enums, u32-count-prefixed vectors. Use `std::bit_cast` and `std::endian`.
- [ ] Unit-test every message type for round-trip fidelity, and test the decoder against truncated, over-long and magic-mismatched frames.
- [ ] Delete every `serialize()` member and all `boost/archive` and `boost/serialization` includes. Drop `serialization` from `find_package(Boost ...)`.

**Collapse the boilerplate**

- [ ] Introduce `template <class T> bool send(Session&, MessageType, const T& payload)` handling encode, framing and the single coalesced write. Replace all ten inline copies in `client.cpp` and `server.cpp`.
- [ ] Give the meta-pair a `struct MetaRef { int owner; std::string key; }` with `encode`/`decode`, replacing the `sprintf("(META %d ")` / `strtok` pair and the four token-counting call sites.

**Coroutine port**

- [ ] Convert `ServerSession`'s `handleReadHeaderSize` → `handleReadHeader` → `handleReadPair`/`handleReadPairUpdate`/`handleReadSubscription` → `startReading` chain into a single `asio::awaitable<void> run()` loop.
- [ ] Convert `ClientSession`'s `handleConnection` → `handleMMandUCandPairMsgs` chain and the reconnect timer into one awaitable loop with a retry delay.
- [ ] Convert `MasterLink::handleUpdateComponentsMsg` and `MasterHub::handleAcceptedConnection` likewise; the master's handshake reads become `co_await`, which removes the blocking-read-in-accept-handler problem outright.
- [ ] Replace the synchronous `boost::asio::write` calls inside handlers with `co_await async_write`, retiring the mutual-blocking deadlock between two nodes filling each other's socket buffers.

**Data structures and API**

- [ ] `PairSpace::pairs_` → `std::map<std::pair<int, std::string>, Pair>` with `std::less<>` for heterogeneous lookup. Delete `isEnd` and `getPairIteratorWithOwnerAndKey`; `getPair` returns `std::optional<Pair>`.
- [ ] `CallbackHandle` → `uint64_t` monotonic counter. Erase from `cbid_to_key_` in `removeCallback`.
- [ ] Merge `subscribed_tuples_`, `owner_id_to_subscribed_pairs_`, `subscription_handle_to_owner_key_` and `subscription_handle_to_key_multiple_` into one handle-keyed registry of `{owner, key}` where a sentinel owner means wildcard. This fixes the collision where a wildcard subscription to a key blocks a later per-owner subscription to the same key — add a test for it.
- [ ] `enum class` with explicit underlying types for `PairType`, `MessageType`, `UpdateComponents::Operation`.
- [ ] `[[nodiscard]]` on `setPair`, `setRemotePair`, `setPairIndirectly`, `setMetaPair`, `initMetaPair` and the send paths; fix the call sites that currently discard the result.
- [ ] Scalar parameters by value instead of `const int&`/`const double&`; `getKey()`/`getValue()` return `const std::string&`; key parameters take `std::string_view`.
- [ ] Delete `Pair::callback_mutex` (declared, never used) and default the copy constructor and assignment operator it forced.
- [ ] Replace the find/erase loops in `PairSpace.cpp` with `std::ranges::find` and `std::erase_if`.
- [ ] Add a `.clang-format` and apply it — the tree currently mixes tabs and spaces and two brace styles.

**Close out**

- [ ] Re-run the ping-pong benchmark against the M1 baseline; record binary-format and coroutine deltas in the README.
- [ ] Update `README.md`: Boost 1.90 / C++20 requirement, the removal of the Python and catkin paths, and the wire-format break.

## Edge cases & risks

- **Wire-format break.** Post-M2 nodes cannot talk to pre-M2 nodes. PairView links the library rather than the protocol, so it needs a rebuild — but rebuild it before considering M2 done, since it is the only external consumer.
- **The coroutine port touches every network path at once**, and it lands on the code with the thinnest coverage. This is why the M1 test suite and the `enable_shared_from_this` lifetime fix are both prerequisites rather than part of it. If the integration fixture turns out to be flaky, stabilize it before starting the port, not during.
- **TSan and Asio.** Asio's internal synchronization is a known source of TSan false positives. Suppress by frame in a checked-in suppressions file; never by dropping the TSan run, which is the only thing that will catch the `sessions_map_` class of bug.
- **`PairSpace` iterator invalidation.** The current `std::vector` invalidates every iterator on `push_back`, and today's code is safe only because it happens to re-look-up after each `addPair`. Any M1 edit near `addPair` must preserve that; the M2 container swap removes the hazard permanently.
- **`std::jthread` and `io_context::run`.** A `stop_token` does not interrupt a blocking `run()`. Shutdown must call `io_context::stop()` first and use the token only for the outer loop.
- **Licensing.** The project is GPLv3; GoogleTest is BSD-3 and compatible. Check any further dependency before adding it.
