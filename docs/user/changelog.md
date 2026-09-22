# Changelog

## Unreleased

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
