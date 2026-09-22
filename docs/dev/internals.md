# Internals

> Describes `include/srnp/local_client.h`, `include/srnp/session.h`,
> `include/srnp/PairSpace.h`, `src/server.cpp`, `src/client.cpp`.

## A publish never leaves the process twice

A component is two halves: a `Server` that owns the pair space and accepts
connections, and a `Client` that holds one outbound connection to every other
component. They share one `PairSpace`, one `io_context` and one process.

They used to be joined by a loopback TCP connection. `setPair` put the pair on
an in-memory queue and sent an **empty** frame to 127.0.0.1 so the server would
wake up and pop it; the server then sent another empty frame back so the client
would fan the pair out. A publish crossed the loopback twice before it reached
the wire, and a round trip between two components cost six socket hops instead
of two. It measured about an order of magnitude worse than a broker doing
strictly more work — see `benchmarks/RESULTS.md`.

Now the halves call each other. `Server` holds a `LocalClient&`:

```cpp
class LocalClient {
  virtual void fanOutPair(const Pair& pair, std::span<const int> subscribers) = 0;
  virtual void fanOutRemoval(const RemovePairRequest&, std::span<const int>) = 0;
  virtual void onWelcome(const MasterMessage& welcome) = 0;
  virtual void onComponentUpdate(const UpdateComponents& update) = 0;
};
```

`Client` implements it. That interface is exactly what the loopback used to
carry: pair updates, removals, and the two master messages. Keeping it an
interface rather than a `Client*` is what lets the tests build a server without
a client behind it.

`PairQueue` and `PairHandoff` are gone with the socket, and so are the three
message types that only ever travelled it: `PairNoCopy`, `PairUpdateOne` and
`AttachClient`.

### What holds the ordering together

`setPair` applies the pair and queues the outgoing frames **under one lock**:

```cpp
std::lock_guard lock(pair_space_.mutex);
const PairEntry& entry = pair_space_.addPair(incoming);
if (!entry.subscribers.empty()) fanOutPair(entry.pair, entry.subscribers);
```

Split that and two threads publishing the same key can apply in one order and
queue in the other, leaving the subscriber holding the older value. The loopback
used to give this ordering for free, because every publish went through one
socket; now the lock states it.

Callbacks are the other half of that. They are **posted** to one strand rather
than run inline, which keeps two things true: `setPair` never runs user code on
its caller's thread, and callbacks arrive in publish order even though four io
threads are available. Before, they happened to be ordered because they all ran
on the single loopback session's strand.

## `FrameChannel`

Everything that touches a socket goes through `FrameChannel`. It does two things:
reads whole frames, and writes whole frames.

**Reading** is a header read of exactly `kHeaderSize` bytes, then a payload read
of exactly `payload_length` bytes. Both are `async_read`, so short reads are
already handled. A malformed header throws and the connection goes.

**Writing** is a queue. `send` appends to `outbox_` and, if no drain coroutine is
running, spawns one on the strand. The drain writes one frame at a time with
`async_write`, so two frames can never interleave on the wire and the buffer
always outlives its write.

The queue is bounded. A subscriber that stops reading would otherwise grow the
publisher's memory without limit, so at `kMaxOutboxFrames` the channel starts
making room, in this order:

1. If the new frame is a pair update for a key **already waiting**, it replaces
   that one in place. The subscriber never saw the older value and now never
   will — which is what last-value-wins means.
2. Otherwise the oldest frame that carries a key is dropped.
3. If nothing in the queue carries a key, the connection is closed.

Step three is the important one. The outbox holds subscriptions and removals
alongside pair updates, and dropping one of those silently corrupts the peer's
idea of the world. Only frames tagged with a `coalesce_key` are ever droppable,
and only `fanOutPair` tags them.

Each frame is built complete in one buffer and written in one call. Splitting the
header and the payload across two writes is what invites a Nagle delay between
them; `TCP_NODELAY` is set as well.

The strand and lifetime rules are in [Threading](threading.md).

## The session classes

There are four, and they are easy to confuse.

| Class | Lives in | Reads | Writes |
|:------|:---------|:------|:-------|
| `MasterHubSession` | the master | one component's messages | that component |
| `MasterLink` | a component's server | the master's updates | the master |
| `ServerSession` | a component's server | one inbound connection | that connection |
| `ClientSession` | a component's client | nothing | any one component |

`ServerSession` is created per accepted connection, one per other component
talking to us. Every one of them is another component's client; our own client
is in this process and does not connect to the acceptor at all.

`ClientSession` is write-only. It connects, replays our subscriptions to that
component, and then only ever writes — replies come back to our server on that
component's own outbound connection, not down this one. Failing to reach a
component is retried every 10 seconds.

Frames sent before a connection comes up are held in `pending_` and flushed in
order once it does, capped at 1024 frames. Without that, anything published in
the moment after learning about a component would be lost.

## `PairSpace`

A `std::map<PairKey, Pair, PairKeyLess>` plus the bookkeeping around it.
`PairKeyLess` is a transparent comparator, so a lookup can be done with a
`PairKeyView` holding a `string_view` and allocates nothing.

Three things live alongside the map:

- `u_subscribers_` — owners subscribed to everything. Copied onto each new pair
  as it is added, which is how a wildcard subscription covers pairs that did not
  exist when it was registered.
- `u_callback_` — one callback run for every pair that changes. One, not a list:
  registering a second replaces the first.
- `callback_owners_` — which pair each callback handle belongs to, so
  `removeCallback` can find it from the handle alone.

### Placeholders

Subscribing to, or registering a callback on, a key that has no value creates a
`Pair` with `Type::Invalid` and an empty value. The subscription or callback
lives on it until a real value arrives, at which point `addPair` overwrites the
value and type and leaves the subscribers and callbacks alone.

Deleting a pair produces the same thing in reverse. `PairSpace::removePair`
erases the entry outright only when nothing is registered against it; otherwise
it strips the value and type back to a placeholder. That is what makes a
subscription survive a deletion, so re-publishing the key still reaches whoever
was already listening.

Placeholders are not pairs and must not escape as if they were. They are filtered
out of wildcard and subscription sends, out of `snapshotPairs()`, and out of
`getPair`. `printPairSpace()` shows them, which is the point of it.

## `PairKey` and `PairKeyView`

`PairKey` owns its string and is what the map stores. `PairKeyView` borrows one
and is what lookups use. `PairKeyLess::is_transparent` is what lets both be
compared against each other; without it every `find` would allocate a string.

## Codec conventions

`encode`/`decode` are free functions found by argument-dependent lookup, one
overload pair per message. `wire::frameOf` picks the right `encode` for whatever
it is handed, and `decodePayload<T>` picks the right `decode` and then insists
the payload was consumed exactly.

Adding a message means: a struct in `CommMessages.h` or `MasterMessages.h`, the
two declarations in `codec.h`, the two definitions in `codec.cpp`, a
`MessageType` value in `wire.h`, and — easy to forget — extending the enum range
check in `decodeHeader` to the new last value.
