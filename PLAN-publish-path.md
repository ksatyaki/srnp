# Plan: collapse the publish path

**Goal:** make a publish reach a subscriber over one TCP connection instead of three, and remove the design faults that structure was hiding — measured, not assumed.

## Why

`Client::setPair` ([client.cpp:392](src/client.cpp:392)) pushes the `Pair` into a shared in-memory queue, then sends an **empty** frame down a TCP connection to its own server. The socket carries no pair data. `Client` and `Server` already share one `PairSpace&`, one `PairQueue&` and one `io_context` ([srnp_kernel.cpp:53](src/srnp_kernel.cpp:53)) — the loopback connection is a cross-thread wakeup implemented as a TCP round trip.

Measured on loopback, Release build, 64 B payload: median round trip 180 µs against 39 µs for mosquitto at QoS 0 and 15 µs for a raw TCP echo. A single hop measured in isolation is ~28 µs; six of them (three per direction) accounts for essentially the whole figure. Hop count is the entire story — the wire codec encodes a pair in ~51 ns and is not involved.

## Approach

Local publishes stop travelling anywhere. `Client::setPair` applies to the pair space and enqueues to each subscriber's session directly. Remote-originated publishes keep their one necessary inbound hop but fan out without bouncing through the local client.

`Server` reaches peer sessions through a narrow `PairSink` interface implemented by `Client`, rather than a raw `Client*`. This keeps the two classes separable — `tests/test_integration.cpp:88` builds a bare `PairQueue` today precisely because they are decoupled, and that seam must survive.

Callbacks keep their documented guarantee ([docs/dev/threading.md](docs/dev/threading.md)): they run on io threads, never on the caller's. Locally-originated callbacks post to one dedicated strand, which reproduces today's ordering exactly — today they all run on the single loopback session's strand. Remote-originated callbacks are untouched; they already run on their peer session's strand.

Three message types exist only to serve the loopback and are deleted: `PairNoCopy`, `PairUpdateOne`, `AttachClient`. `PairUpdate` and `PairRemoved` stay — those are real peer-to-peer frames.

## Changes

- `include/srnp/pair_sink.h` — **new.** One interface: fan a pair out to a subscriber list, and announce a removal to a subscriber list.
- `src/client.cpp`, `include/srnp/client.h` — `setPair`/`removePair` apply locally and fan out directly. `Client` implements `PairSink`. Drop `my_server_session_`, the self-connection, and `handlePairUpdate`/`handlePairRemoved` relays. Add the local-callback strand.
- `src/server.cpp`, `include/srnp/server.h` — `Server` holds a `PairSink&`. `sendPairUpdate` calls the sink instead of the loopback. Delete `handleLocalPair` and `attachClient`.
- `include/srnp/PairQueue.h` — **delete.** Its only purpose was pairing an empty frame with a queued object.
- `include/srnp/wire.h` — remove the three loopback-only message types, bump `kVersion` to 2.
- `include/srnp/session.h`, `src/session.cpp` — bounded outbox with key coalescing (see below).
- `include/srnp/Pair.h`, `include/srnp/PairSpace.h`, `src/PairSpace.cpp` — move `subscribers_` and `callbacks_` out of `Pair` into a `PairEntry` beside it in `PairSpace`.
- `src/srnp_kernel.cpp` — drop `pair_queue_`. The `pairs.back().callbacks_.clear()` at line 163 deletes itself.
- `benchmarks/` — **new**, from the existing scratch harness.
- `tests/test_pair_space.cpp` — asserts directly on `Pair::subscribers_`/`callbacks_`; rewrite against the new accessors.
- `docs/dev/internals.md`, `docs/dev/threading.md`, `docs/dev/protocol.md` — the handoff, its locking rule, and the three message types all go.

## Milestone 1 — harness, then the collapse

The harness lands first so the baseline is captured against unmodified code.

- [ ] Move the four harnesses (`srnp_bench`, `mqtt_bench`, `zmq_bench`, `tcp_bench`), `common.hpp`, the driver and README into `benchmarks/`.
- [ ] Add `benchmarks/CMakeLists.txt` behind `option(SRNP_BENCHMARKS "Build the comparison benchmarks" OFF)`. The SRNP and raw-TCP harnesses build whenever the option is on; the mosquitto and zeromq comparators each skip with a status message if their library is not found.
- [ ] Set a default `CMAKE_BUILD_TYPE` of `RelWithDebInfo` in the top-level `CMakeLists.txt` when the user supplied none. The current default is an unoptimised build, which reads ~4x slower than Release and would make every number below meaningless.
- [ ] Record the pre-change baseline into `benchmarks/RESULTS.md`: latency p50/p90/p99/p99.9 and throughput at 64 B, 1 KB, 64 KB, for all four systems, with the machine and governor noted.
- [ ] Add `PairSink` and make `Client` implement it. `Server` takes a `PairSink&` at construction.
- [ ] Rewrite `Client::setPair`: under `pair_space_.mutex`, apply the pair, copy out the subscriber list, encode the frame once; release the lock; enqueue to each subscriber session; post callbacks to the local-callback strand. Holding the lock across apply-and-capture is what keeps per-key ordering — do not split it.
- [ ] Rewrite `Client::removePair` the same way.
- [ ] Point `ServerSession::sendPairUpdate` and the removal path at the sink. Delete `handleLocalPair` and `attachClient`.
- [ ] Delete the self-connection from the `Client` constructor and `PairQueue.h`, and drop `pair_queue_` from `KernelInstance`, `Client` and `Server`.
- [ ] Remove `PairNoCopy`, `PairUpdateOne` and `AttachClient` from `wire.h`; bump `kVersion` to 2. Confirm the decoder rejects a mismatched version with a clean error rather than a garbled frame — a v1 peer cannot talk to a v2 peer and should be told so.
- [ ] Update `tests/test_integration.cpp` for the new constructor signatures.
- [ ] Re-run the harness. Append the post-change column to `benchmarks/RESULTS.md`.
- [ ] Update `docs/dev/internals.md`, `threading.md` and `protocol.md`.

**Verification:** `ctest --test-dir build --output-on-failure` passes, and median round trip at 64 B is at least halved against the recorded baseline. One commit.

## Milestone 2 — backpressure and the per-message copy

- [ ] Change the outbox element from a bare frame to `{bytes, optional<PairKey> coalesce_key}`; only `PairUpdate` frames carry a key. Switch the outbox from `std::deque` to `std::list` plus a `PairKey -> iterator` index so coalescing is O(1) without invalidation.
- [ ] Cap the outbox. On overflow, in order: replace an already-queued entry with the same `coalesce_key` in place; else drop the oldest entry that has a `coalesce_key`; else close the connection and log it. Control frames — subscriptions, removals — are never droppable; dropping one silently corrupts peer state.
- [ ] Move `subscribers_` and `callbacks_` out of `Pair` into a `PairEntry` held by `PairSpace`, so `Pair` carries only data.
- [ ] Make `applyAndNotify` cost one allocation per update and copy no callback map and no subscriber vector. Build the `shared_ptr<const Pair>` by moving the incoming pair rather than copying the stored one.
- [ ] Give `registerCallback(kAnyOwner, "*")` a real handle. Today it overwrites the previous universal callback without warning and returns `kInvalidCallbackHandle`, so it can never be cancelled ([client.cpp:499](src/client.cpp:499)). Hold a keyed collection and return a cancellable handle.
- [ ] Rewrite the `tests/test_pair_space.cpp` assertions against the new accessors.
- [ ] Add a test that a subscriber which stops reading does not grow the publisher without bound, and that it receives the *latest* value per key once it resumes rather than a stale backlog.
- [ ] Re-run the harness and append the final column to `benchmarks/RESULTS.md`.

**Verification:** `ctest` passes, the new backpressure test passes, throughput at 64 B has not regressed against milestone 1. One commit.

## Edge cases & risks

- **Ordering.** Per-subscriber FIFO survives: each channel keeps one outbox on one strand. The hazard is two threads publishing the same key and interleaving between apply and enqueue — the pair-space lock must span both.
- **Callback ordering.** A single local-callback strand reproduces today's global ordering for locally-originated pairs. Posting to the bare `io_context` instead would let two rapid `setPair` calls deliver out of order across four threads.
- **Reentrancy.** With callbacks posted rather than called inline, a callback that itself calls `setPair` no longer runs beneath the publisher's stack. Confirm no test depends on the old inline timing.
- **Coalescing changes what a slow subscriber sees.** It receives the current value and never the intermediate ones. That is correct for a last-value-wins blackboard and wrong for anything treating a pair as an event stream — say so in the user docs.
- **`kVersion` bump is a hard break.** Mixed-version components stop talking. Acceptable here; note it in the changelog.
- **Absolute numbers are soft.** Laptop, `powersave` governor, no core pinning. Compare against the recorded baseline on the same machine, not against the figures quoted above.
