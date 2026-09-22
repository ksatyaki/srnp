# Results

One machine, loopback only. Both columns were measured the same way, minutes
apart, so the comparison between them is meaningful even though the absolute
numbers are soft.

    13th Gen Intel Core i7-13700HX, 24 threads, powersave governor, no pinning
    Fedora 44, gcc, -O3, Boost.Asio
    mosquitto 2.1.2 (QoS 0), libzmq 4.3.5
    5000 round trips after 1000 discarded, single message in flight

Raw CSV: [`results-before.csv`](results-before.csv),
[`results-after.csv`](results-after.csv).

**before** is commit `31d7218`, where a publish crossed a loopback TCP
connection twice before reaching the wire. **after** is the change that removed
it. Nothing else about the machine or the harness differs.

## Round-trip latency, microseconds

| payload | | p50 | p90 | p99 | p99.9 |
|:--|:--|--:|--:|--:|--:|
| 64 B | srnp before | 212.1 | 268.4 | 509.5 | 722.0 |
| | **srnp after** | **101.6** | **129.6** | **262.1** | **386.9** |
| | mqtt | 55.4 | 79.2 | 289.0 | 660.7 |
| | zeromq | 25.1 | 43.9 | 104.1 | 277.5 |
| | raw tcp *(floor)* | 14.1 | 16.4 | 20.9 | 91.7 |
| 1 KB | srnp before | 209.2 | 263.5 | 450.0 | 709.6 |
| | **srnp after** | **71.8** | **117.1** | **243.8** | **382.1** |
| | mqtt | 56.0 | 75.2 | 176.9 | 256.4 |
| | zeromq | 55.3 | 68.4 | 168.3 | 287.6 |
| | raw tcp *(floor)* | 10.5 | 14.6 | 22.6 | 120.7 |
| 64 KB | srnp before | 411.1 | 698.7 | 964.3 | 1162.8 |
| | **srnp after** | **366.4** | **552.4** | **708.5** | **938.2** |
| | mqtt | 125.0 | 330.9 | 529.2 | 734.3 |
| | zeromq | 48.4 | 95.5 | 161.1 | 332.6 |
| | raw tcp *(floor)* | 22.2 | 27.1 | 53.2 | 163.8 |

Median round trip roughly halved at 64 B and fell by two thirds at 1 KB. The
tail moved with it, which matters more: p99.9 at 64 B went from 722 µs to
387 µs.

srnp now sits between mosquitto and zeromq at 1 KB, and is still roughly twice
mosquitto at 64 B. It has not caught the raw TCP floor and is not going to
without a different design — the floor does no pair-space work, holds no lock
and runs no callbacks.

### The hop, in isolation

Publishing and waiting for a callback on the same node, one way:

| | p50 | p99 |
|:--|--:|--:|
| before | 28.5 µs | 140.3 µs |
| after | 4.8 µs | 7.7 µs |

This is the whole change in one number. That step used to be a TCP round trip
to 127.0.0.1; it is now a lock, a queue and a post to a strand.

## Sustained throughput

| payload | | published | delivered | msg/s delivered |
|:--|:--|--:|--:|--:|
| 64 B | srnp before | 100000 | 100000 | 73081 |
| | **srnp after** | 100000 | 22756 | **204126** |
| | mqtt | 100000 | 100000 | 788215 |
| | zeromq | 100000 | 100000 | 2036348 |
| | raw tcp | 100000 | 100000 | 224532 |
| 1 KB | srnp before | 100000 | 100000 | 69574 |
| | **srnp after** | 100000 | 29409 | **203332** |
| | mqtt | 100000 | 100000 | 490328 |
| | zeromq | 100000 | 100000 | 1045569 |
| | raw tcp | 100000 | 100000 | 208691 |
| 64 KB | srnp before | 5000 | 5000 | 21922 |
| | **srnp after** | 5000 | 5000 | **39433** |
| | mqtt | 5000 | 5000 | 28164 |
| | zeromq | 5000 | 5000 | 67407 |
| | raw tcp | 5000 | 5000 | 78755 |

**Read the delivered column before the rate column.** At 64 B and 1 KB the
publisher outruns the subscriber, the outbox fills, and updates to the same key
coalesce — so roughly three in four published values are superseded before the
subscriber ever sees them. That is the bounded-outbox policy working as
intended for a last-value-wins blackboard, and it is a real behaviour change:
**srnp no longer promises to deliver every intermediate value to a subscriber
that cannot keep up.** The subscriber always reaches the current value; it does
not see every step on the way there.

If you are treating a pair as an event stream, this is not the middleware for
it, and that was true in spirit before — the change just makes it explicit and
bounds the memory instead of queueing without limit.

The 64 KB row has no coalescing: 5000 messages never fill the queue. It is the
one row where before and after measure the same thing, and throughput there
went from 21922 to 39433 msg/s.

## Wire codec

Never the bottleneck, before or after. Encoding a pair with a 64 B value costs
about 99 ns and decoding about 20 ns, against round trips measured in tens of
microseconds. Any future work should start somewhere else.

## What these numbers are not

- One machine, one loopback, `powersave` governor, no core pinning, no isolated
  CPUs. Tails are noisy. Ratios are the part to trust.
- One publisher, one subscriber, one key. Nothing here explores fan-out to many
  subscribers, pair-space size, or component count — all of which the design
  has opinions about and none of which is measured.
- Closed-loop latency at zero load, and saturating throughput. Neither is an
  open-loop latency-under-load test, so nothing here speaks to coordinated
  omission.
