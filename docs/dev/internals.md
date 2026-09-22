# Internals

> Describes `include/srnp/PairQueue.h`, `include/srnp/session.h`,
> `include/srnp/PairSpace.h`, `src/server.cpp`, `src/client.cpp`.

## The empty `PairUpdate` frame

This is the most surprising thing in the codebase, and it reads like a bug until
you know what it is.

When our server has a pair to forward, it does this:

```cpp
auto guard = server_.pairQueue().updates.lock();
server_.pairQueue().updates.push(pair);
client_session->send(wire::frame(type, {}, to_one ? subscriber : kAnyOwner));
```

The frame has **no payload**. The pair itself goes into `PairQueue::updates`, an
in-memory queue, and the client pops it when the frame arrives. The same trick
runs the other way: `setPair` pushes to `PairQueue::outgoing` and sends an empty
`PairNoCopy`.

It is not a bug. Our client and our server are in the same process, connected by
a loopback socket only because that gives the server one uniform way to be
talked to. Serialising a pair, writing it through the kernel, and parsing it back
out would be pure waste when a pointer move would do. So the socket carries the
*notification* and the queue carries the *data*.

The frame still has to exist: it is what wakes the reader and tells it which kind
of handoff this is, and for `PairUpdateOne` its header names the one subscriber.

### Why push and send are under one lock

```cpp
[[nodiscard]] std::unique_lock<std::mutex> lock() { return std::unique_lock(mutex_); }
```

`PairHandoff::lock()` hands the caller the mutex, and the caller holds it across
both the push and the send. Without that, two threads could push in one order and
send in the other, and each reader would pop a pair belonging to someone else's
frame. Every pair after that would be wrong, silently.

If a reader ever pops an empty queue, a frame arrived without its pair and
something has broken that rule. That is why the handlers log an error rather than
returning quietly.

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
| `ClientSession` | a component's client | our own server only | any one component |

`ServerSession` is created per accepted connection. It cannot tell in advance
whether the peer is our own client or another component's — both connect to the
same acceptor — so our client identifies itself with `AttachClient`, and
`Server::attachClient` remembers that one session. It is the only one the server
can send to, because it is the only path back out to the network.

`ClientSession` has two modes, set by `is_our_own_server_`. The session to our
own server connects, sends `AttachClient`, and reads forever. A session to
another component connects, replays our subscriptions to it, and then only ever
writes. Failure is treated differently too: not reaching another component is
retried every 10 seconds, while not reaching our own server is fatal, since it is
in this process.

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
