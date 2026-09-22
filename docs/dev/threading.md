# Threading

> Describes `src/session.cpp`, `src/server.cpp`, `src/client.cpp`,
> `include/srnp/session.h`, `include/srnp/PairSpace.h`.

These are the rules the networking code depends on. Most of them are not
enforceable by the compiler, and breaking one produces a race that only the
thread sanitizer will find.

## One io context, four threads

A component has one `asio::io_context`. `Server::startWorkers` runs it on four
`std::jthread`s. Four rather than one so a slow user callback — which runs on
whichever io thread delivered the pair — does not stall everything else.

Four threads running one io context means any handler can run on any thread, and
two handlers can run at once. Everything below follows from that.

## A socket is touched only on its own strand

Asio does not allow concurrent operations on one socket. Every socket in srnp is
owned by a `FrameChannel`, and every `FrameChannel` has a strand. Reads, writes
and the close all run on that strand, so they are serialised against each other
even though four threads are available.

Strands are created with `asio::make_strand(io)` — one per connection, never
shared between connections.

### The three operations, and where each runs

| Operation | Callable from | Runs on |
|:----------|:--------------|:--------|
| `FrameChannel::read` | the channel's strand only, inside a coroutine | that strand |
| `FrameChannel::send` | any thread | queues under a mutex; the write itself runs on the strand |
| `FrameChannel::close` | any thread | posts to the strand; the socket is closed there |

`read` is the strict one. Whoever creates a channel must spawn its read loop on
`channel->strand()`, and nothing else may call `read`.

`send` takes `outbox_mutex_`, appends the frame, and spawns a drain coroutine on
the strand if one is not already running. The caller returns as soon as the frame
is queued, never having touched the socket.

## Closes are posted, never performed inline

`close()` sets an atomic flag, clears the outbox, and then *posts* the actual
`shutdown`/`close` to the strand:

```cpp
asio::post(strand_, [self = shared_from_this()] {
  boost::system::error_code ignored;
  self->socket_.shutdown(tcp::socket::shutdown_both, ignored);
  self->socket_.close(ignored);
});
```

Closing the socket on the calling thread would race a read or a write already in
flight on another. Posting puts the close in line behind them on the strand
instead. The captured `shared_from_this()` is what keeps the channel alive until
that runs.

The atomic flag is set immediately rather than on the strand, so a `send` racing
the close is dropped straight away instead of being queued onto a socket that is
about to go.

## The acceptor has its own strand

An acceptor is a socket too, and `Server::stop()` can be called from any thread —
including while `async_accept` is in flight. So the accept loop runs on
`acceptor_strand_`, and `stop()` posts the acceptor's close to that same strand.
Same rule as every other socket, same reason.

## Every mutex

| Mutex | Guards | Held across |
|:------|:-------|:------------|
| `PairSpace::mutex` | the whole pair space: pairs, subscribers, callbacks, handles | a lookup plus its update; **never** a user callback |
| `FrameChannel::outbox_mutex_` | the send queue and the `writing_` flag | a push, or a pop; never a socket operation |
| `ClientSession::channel_mutex_` | the channel pointer and the pending-frame deque | checking the channel and queueing, as one step |
| `Client::state_mutex_` | the session map, the component map, the subscription map | map lookups only |
| `Client::ready_mutex_` | pairs with `ready_changed_` for `waitUntilReady` | the condition-variable wait |
| `Server::client_session_mutex_` | the pointer to our own client's session | a read or a write of that pointer |
| `MasterHub::mutex_` | the master's session map | building a reply, or collecting send targets |
| `PairHandoff::mutex_` | one pair queue | a push and the send that announces it |

### `PairSpace::mutex` is public on purpose

Callers routinely need a lookup and an update to be one atomic step, so the mutex
is public and the caller locks it. `PairSpace`'s own methods do not lock.

The one thing that must not happen under it is running a user callback.
`ServerSession::applyAndNotify` is the pattern: apply the pair and take a
snapshot under the lock, release it, then run the callbacks.

```cpp
{
  std::lock_guard lock(server_.pairSpace().mutex);
  snapshot = std::make_shared<const Pair>(server_.pairSpace().addPair(pair));
  universal = server_.pairSpace().u_callback_;
}
if (universal) universal(snapshot);
for (const auto& [handle, callback] : snapshot->callbacks_) callback(snapshot);
```

The snapshot is a `shared_ptr<const Pair>`, so the callback sees a value that
cannot change under it and does not need the lock to read.

`ServerSession::handleSubscription` uses the same shape for a different reason:
it collects the pairs to send under the lock and sends them after releasing it.

## Never hold two locks

No code path in srnp holds two of the mutexes above at once, and none should.
That is what keeps the locking order question from arising at all.

## What runs where

- **User callbacks** — on an io thread, outside every lock. Several can run at
  once, on different threads, for different pairs. Anything a callback touches
  needs its own locking.
- **`initialize`** — on the caller's thread. The master registration inside it is
  synchronous blocking I/O on the socket, before any worker thread exists.
- **`setPair`, `getPair` and the rest** — on the caller's thread. They queue work
  or take the pair-space lock briefly; none of them waits for the network.
- **`shutdown`** — on the caller's thread, and it joins the io threads before
  releasing anything they can reach.

## Atomics

`Client::ready_`, `Client::owner_id_`, `Server::owner_id_`, `Server::stopped_`,
`FrameChannel::closed_`, `ClientSession::closing_` and `MasterLink::closing_` are
atomics rather than being under a mutex, because they are read on hot paths where
the reader only needs the current value.

`ready_` is the one with ordering that matters: it is stored with
`memory_order_release` and read with `memory_order_acquire`, so a thread seeing
`ready() == true` also sees the owner id that was written before it. It is stored
under `ready_mutex_` as well, so `waitUntilReady` cannot miss the notification.
