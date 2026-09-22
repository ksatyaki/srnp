# SRNP middleware sneak-peek benchmark

Four systems, the same two tests, the same machine, same process model
(two processes over loopback).

| system    | shape                | what it is                                         |
|:----------|:---------------------|:---------------------------------------------------|
| `raw-tcp` | direct, 1 hop        | length-prefixed frames, `TCP_NODELAY`. The floor.   |
| `zeromq`  | brokerless PUB/SUB   | libzmq 4.3.5. Closest architectural match to SRNP.  |
| `mqtt`    | broker, 2 hops       | mosquitto 2.1.2, QoS 0, libmosquitto client.        |
| `srnp`    | this repo            | `setPair` -> subscriber callback.                   |

## Tests

**Latency** — closed-loop ping/pong, one message in flight, round trip measured
on `CLOCK_MONOTONIC` in the publisher so no clock sync is needed. 1000 warm-up
iterations discarded, 5000 measured. Reported as p50/p90/p99/p99.9/max.

**Throughput** — one-way, publisher saturating, subscriber counting. Reported
as delivered messages/s and MB/s. The publisher idles before sending an `eof`
marker so in-flight messages are not miscounted as lost.

This is a *saturating* throughput test, not an open-loop latency-under-load
test. It therefore says nothing about coordinated omission: latency here is
measured at zero load only.

## Building and running

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSRNP_BENCHMARKS=ON
    cmake --build build -j
    ./build/bin/run_bench.sh > benchmarks/results.csv

`run_bench.sh` starts and stops `srnp-master` itself. It does not start an MQTT
broker — run one on 127.0.0.1:1883 or the mqtt rows are skipped. The mosquitto
and zeromq harnesses are only built if their libraries are found; the srnp and
raw-tcp ones always are.

**Build Release when you measure.** An unoptimised build reads about four times
slower through the transport, which is not a fact about the design.

## Caveats

- One laptop, `powersave` governor, no core pinning, no isolated CPUs. The
  absolute numbers are soft and the tails are noisy; the ratios between systems
  are the part worth reading.
- Loopback only. A real network would compress the differences.
- One publisher, one subscriber, one key. Nothing here explores fan-out,
  pair-space size, or component count.
