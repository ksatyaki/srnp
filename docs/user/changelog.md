# Changelog

## Unreleased

- **A publish no longer crosses a loopback socket twice before reaching the
  wire.** A component's client and server are in the same process and now call
  each other directly instead of over a TCP connection to 127.0.0.1. Median
  round-trip latency roughly halved at 64 B and fell by two thirds at 1 KB; see
  [`benchmarks/RESULTS.md`](https://github.com/ksatyaki/srnp/blob/master/benchmarks/RESULTS.md).
- **The wire protocol is version 2 and will not talk to version 1.** Three
  message types are gone — `PairNoCopy`, `PairUpdateOne` and `AttachClient` —
  because all three existed only to serve that loopback connection. A version 1
  peer is rejected on the version check rather than misparsing a frame. Rebuild
  every component.
- **A subscriber that cannot keep up now loses intermediate values rather than
  memory.** The send queue per connection is bounded. At the cap, a new update
  for a key already waiting replaces it, so the subscriber gets the current
  value instead of a stale backlog; subscriptions and removals are never
  dropped. Before this, a subscriber that stopped reading grew the publisher's
  memory without limit. If you were relying on receiving every intermediate
  value of a fast-changing pair, you no longer will — see
  [Concepts](concepts.md#delivery).
- `registerCallback(owner, "*", fn)` returns a real, cancellable handle, and you
  may register more than one. It used to return `kInvalidCallbackHandle` and
  silently replace any previous wildcard callback.
- Callbacks for pairs this node publishes now run on one strand, in publish
  order, and never on the thread that called `setPair`.
- `cmake` with no `CMAKE_BUILD_TYPE` now defaults to `RelWithDebInfo` instead of
  an unoptimised build. The old default read about four times slower through the
  transport.
- A benchmark harness in [`benchmarks/`](https://github.com/ksatyaki/srnp/tree/master/benchmarks),
  behind `-DSRNP_BENCHMARKS=ON`, comparing srnp against mosquitto, zeromq and a
  raw TCP floor.
- `Pair` no longer carries the subscriber list and callback map. They were this
  component's bookkeeping, not part of the value, and every update copied them.
  `PairSpace::find` now returns a `PairEntry`, which holds the `Pair` alongside
  them.

- **[PairView](pairview.md)**, a window onto the pair space, built on Dear ImGui
  and replacing the old Qt viewer. It shows every pair grouped by owner, and
  posts, deletes and subscribes. Built by default when GLFW and the vendored
  Dear ImGui submodule are present, and skipped with a status line when they are
  not.
- Pairs can be deleted. `removePair(key)` drops one of your own and tells its
  subscribers; `removeRemotePair(owner, key)` drops one on another component.
  Subscriptions survive a deletion, so re-publishing the key reaches the same
  subscribers without re-subscribing.
- Two new message types, `RemovePair` and `PairRemoved`. **A component built
  against this version can delete pairs on one built before it only if that one
  also understands the new types** — an older peer logs an unknown-message-type
  warning and drops the frame.
- `snapshotPairs()` returns a copy of the whole pair space, and `components()`
  returns every other component the master has told us about.
- `getPair` no longer returns an empty pair for a key that has only a
  subscription or a callback registered against it. Those are bookkeeping
  entries, and they now read as absent.

## 0.2.0

- Requires C++20 and a current Boost. The old Boost-1.54-era code no longer
  compiled at all: `io_service`, `asio::strand` and the rest are long gone.
- **The wire format changed and is not compatible with 0.1.** Messages are now a
  fixed 16 byte binary header plus a length-prefixed binary payload, instead of
  Boost text archives. Rebuild every component.
- `initialize` throws `srnp::InitError` instead of calling `exit`, and `shutdown`
  returns instead of ending the process.
- The Python wrapper (Boost.Python against Python 2.7, incomplete and not built)
  and the ROS 1 catkin files are gone.
