# API reference

Everything here is in namespace `srnp` and declared in
[`include/srnp/srnp_kernel.h`](https://github.com/ksatyaki/srnp/blob/master/include/srnp/srnp_kernel.h)
or
[`include/srnp/meta_pair_callback.hpp`](https://github.com/ksatyaki/srnp/blob/master/include/srnp/meta_pair_callback.hpp).
One include covers both:

```cpp
#include <srnp/srnp_kernel.h>
#include <srnp/meta_pair_callback.hpp>
```

Unless an entry says otherwise, every function is safe to call from any thread
and must be called between `initialize` and `shutdown`. Calling one outside that
window throws `InitError`.

Grouped by what you are trying to do, not by header.

## Lifecycle

### `void initialize(int argc, char* argv[])`

Starts the node. Reads `SRNP_MASTER_IP` and `SRNP_MASTER_PORT` from the
environment, and accepts `--owner-id <n>` in the arguments.

Blocks until the master assigns an owner id, up to 10 seconds. Throws
`InitError` if the variables are missing, the master cannot be reached, the id
never arrives, or this process already has a node.

### `void initialize(std::string_view master_ip, std::string_view master_port, int desired_owner_id = kAnyOwner, std::string_view node_name = "srnp")`

The same, with the master's address passed directly instead of through the
environment. `node_name` appears only in the startup log line.

### `void shutdown()`

Stops the node, joins its threads, and releases everything. Safe to call more
than once, and safe to call on a node that never started.

### `bool ok()`

True between a successful `initialize` and `shutdown`.

### `int getOwnerID()`

This component's owner id.

## Writing pairs

### `bool setPair(std::string_view key, std::string_view value, Pair::Type type = Pair::Type::String)`

Publishes one of our own pairs, creating it or replacing its value. Returns
before the value has been applied — the write is queued, not synchronous, so an
immediate `getPair` may still see the old value.

Returns false only on a node that never finished starting, so on a healthy node
the result is always true.

The owner is always us. There is no way to publish under someone else's id.

### `bool setRemotePair(int owner, std::string_view key, std::string_view value, Pair::Type type = Pair::Type::String)`

Pushes a pair to another component, which then owns it. Returns false if we have
no connection to that component — usually because the master has not told us
about it yet. Retrying a moment later is the normal fix.

### `bool removePair(std::string_view key)`

Deletes one of our own pairs and tells every subscriber it is gone. Deleting a
key that was never published succeeds and does nothing. The return value means
the same as `setPair`'s.

Subscriptions outlive the deletion. Publishing the key again reaches whoever was
already subscribed, with no need to re-subscribe.

### `bool removeRemotePair(int owner, std::string_view key)`

Deletes a pair on another component. Passing our own id is the same as
`removePair`. Returns false if we are not connected to that component.

## Reading pairs

### `std::optional<Pair> getPair(int owner, std::string_view key)`

A copy of one pair, or `nullopt` if we do not have it. For another component's
pair you must be subscribed first; without a subscription the value never
arrives and this always returns `nullopt`.

A key that has only a subscription or a callback registered against it, and no
value, reads as `nullopt` — as does a key that was deleted.

### `std::vector<Pair> snapshotPairs()`

A copy of every pair we hold, taken under the pair-space lock so it stays valid
afterwards. Keys with no value are left out, and the copies carry no callbacks.

This is how to walk the pair space; there is no way to iterate it safely in
place.

### `void printPairSpace()`

Prints every pair, including the valueless placeholders `snapshotPairs` skips, to
standard output. A debugging aid.

### `std::map<int, ComponentInfo> components()`

Every other component the master has told us about, keyed by owner id. Each
`ComponentInfo` holds `owner`, `ip` and `port`.

Ourselves excluded: the master never lists a component to itself. An entry
appears when the master announces a component and disappears when it
disconnects.

## Subscriptions

### `SubscriptionHandle registerSubscription(int owner, std::string_view key)`

Asks a component to send us that pair whenever it changes, and sends the current
value straight away if it has one.

`owner` may be `kAnyOwner`, meaning that key on every component, including ones
that join later. `key` may be `"*"`, meaning every key that component owns.

**Returns `kInvalidSubscriptionHandle` if we are already subscribed to that exact
`<owner, key>`**, having done nothing. It is not an error, but the existing
subscription is unchanged, and the handle you would have cancelled with is the
one from the first call.

Subscribing before the other component exists is fine. Subscriptions are
replayed to components as they connect.

### `SubscriptionHandle registerSubscription(std::string_view key)`

Short for `registerSubscription(kAnyOwner, key)`.

### `void cancelSubscription(SubscriptionHandle handle)`

Cancels the subscription that handle came from. Warns and does nothing if the
handle is unknown.

### `void cancelSubscription(int owner, std::string_view key)`

The same, found by what was subscribed to rather than by handle.

### `void cancelSubscription(std::string_view key)`

Short for `cancelSubscription(kAnyOwner, key)`. This cancels the wildcard
subscription on that key, not the per-owner ones — those are separate
subscriptions and must be cancelled separately.

## Callbacks

### `CallbackHandle registerCallback(int owner, std::string_view key, Pair::CallbackFunction callback_fn)`

Runs `callback_fn` whenever that pair changes. The callback takes a
`Pair::ConstPtr` — a shared pointer to an immutable copy of the new value.

`owner` and `key` must match exactly. `kAnyOwner` is **not** a wildcard here:
a callback registered against it waits for a pair genuinely owned by -1 and never
fires. Subscriptions accept `kAnyOwner`; callbacks do not.

A callback on another component's pair needs a subscription behind it. The
subscription is what delivers the value; the callback only reacts.

Callbacks run on an srnp io thread, outside the pair-space lock. Several may run
at once on different threads, so anything a callback touches needs its own
locking.

### `registerCallback(owner, "*", fn)` — the wildcard form

Passing `"*"` as the key registers `fn` for **every** pair the node applies,
whatever its owner or key, and ignores the `owner` argument entirely. Use it when
the publisher's id is not known in advance and filter inside the callback.

Two things to know:

- **It returns `kInvalidCallbackHandle`, so it can never be cancelled.** There is
  no handle and `cancelCallback` has nothing to work with.
- There is only one of it. Registering a second wildcard callback replaces the
  first.

### `void cancelCallback(CallbackHandle handle)`

Removes a callback. Warns and does nothing if the handle is unknown or is
`kInvalidCallbackHandle`.

## Meta-pairs

A meta-pair is a pair whose value is `(META <owner> <key>)`, naming another pair.
See [Concepts](concepts.md#meta-pairs).

### `bool setMetaPair(int meta_owner, std::string_view meta_key, int owner, std::string_view key)`

Points the meta-pair at `<owner, key>`. Writes locally or remotely depending on
whether `meta_owner` is us.

### `bool initMetaPair(int meta_owner, std::string_view meta_key)`

Creates a meta-pair pointing nowhere, with the value `(META -1 NULL)` and type
`Pair::Type::Meta`.

### `std::optional<Pair> getPairIndirectly(int metaowner, std::string_view metakey)`

Reads the meta-pair, follows it, and returns the target. `nullopt` if the
meta-pair is missing, is not a meta-pair, or points at something we do not hold —
including the case of one that points nowhere.

### `bool setPairIndirectly(int metaowner, std::string_view metakey, std::string_view value)`

Follows the meta-pair and writes the target, local or remote as needed. False if
the meta-pair is missing or is not a meta-pair.

### `void registerMetaCallback(int meta_owner_id, std::string_view meta_pair_key, Pair::CallbackFunction cb)`

Subscribes to the meta-pair, follows it to its target, and subscribes to that
too, running `cb` on the target's changes. When the meta-pair is repointed, the
subscription and callback move to the new target.

This is the reason meta-pairs exist: what a component watches becomes something
another component can change at runtime.

### `void registerMetaSubscription(int meta_owner_id, std::string_view meta_pair_key)`

The same tracking with no callback, so the target's value simply keeps arriving
in the local pair space.

### `void cancelMetaCallback(int meta_owner_id, std::string_view meta_pair_key)`

Drops the meta-pair's own subscription and whatever it currently points at.

### `void cancelMetaSubscription(int meta_owner_id, std::string_view meta_pair_key)`

The same, for a tracker registered with `registerMetaSubscription`.

## Types and constants

### `class Pair`

| Member | Meaning |
|:-------|:--------|
| `getOwner()` | The component that published it. |
| `getKey()` | The key. |
| `getValue()` | The value, as a `std::string` that may hold arbitrary bytes. |
| `getType()` | `Pair::Type`. |
| `getWriteTime()` | When the owner's node applied the value. |
| `getExpiryTime()` | Carried on the wire and readable, but **acted on by nothing**. |
| `Pair::Ptr`, `Pair::ConstPtr` | `shared_ptr<Pair>` and `shared_ptr<const Pair>`. |
| `Pair::CallbackFunction` | `std::function<void(const Pair::ConstPtr&)>`. |

### `enum class Pair::Type`

`String`, `Bytes`, `Meta`, and `Invalid`. `Invalid` is internal: it marks an
entry that holds a subscription or a callback but no value. Never publish it.

### `struct ComponentInfo`

`int owner`, `std::string ip`, `std::string port`.

### `struct InitError : std::runtime_error`

Thrown by `initialize`, and by any call made with no node running.

### Constants

| Constant | Value | Meaning |
|:---------|:------|:--------|
| `kAnyOwner` | `-1` | "Any owner" in a subscription. Not a wildcard for callbacks. |
| `kInvalidCallbackHandle` | `0` | A callback registration that produced no cancellable handle. |
| `kInvalidSubscriptionHandle` | `0` | A subscription that was refused as a duplicate. |

### Logging

`bool srnp_print_setup(std::string_view level)`, from `<srnp/srnp_print.h>`, sets
the minimum level printed: `trace`, `debug`, `info`, `warning`, `error`, `fatal`,
`off`, in any case. Returns false and changes nothing for an unknown name.
