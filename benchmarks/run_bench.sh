#!/usr/bin/env bash
# Runs every harness that was built through the same matrix and writes CSV to
# stdout. Run it from the directory the benchmarks were built into.
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSRNP_BENCHMARKS=ON
#   cmake --build build -j
#   ./build/bin/run_bench.sh > benchmarks/results.csv
#
# srnp-master is started and stopped here. An MQTT broker is not: start one on
# 127.0.0.1:1883 yourself, or the mqtt rows are skipped.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
MASTER_PORT=${SRNP_MASTER_PORT:-12321}
export SRNP_MASTER_IP=${SRNP_MASTER_IP:-127.0.0.1}
export SRNP_MASTER_PORT="$MASTER_PORT"

LAT_ITERS=${LAT_ITERS:-5000}
LAT_WARMUP=${LAT_WARMUP:-1000}
PAYLOADS=${PAYLOADS:-"64 1024 65536"}

master_pid=""
finish() {
  pkill -f "$HERE/(srnp|mqtt|zmq|tcp)_bench (pong|sub)" 2>/dev/null
  [ -n "$master_pid" ] && kill "$master_pid" 2>/dev/null
}
trap finish EXIT

master="$HERE/srnp-master"
[ -x "$master" ] || master=$(command -v srnp-master)
if [ -z "$master" ] || [ ! -x "$master" ]; then
  echo "cannot find srnp-master; build it first" >&2
  exit 1
fi
"$master" "$MASTER_PORT" >/dev/null 2>&1 &
master_pid=$!
sleep 1

systems=""
for s in tcp zmq mqtt srnp; do
  [ -x "$HERE/${s}_bench" ] && systems="$systems $s"
done
if ! command -v mosquitto >/dev/null 2>&1 && ! (exec 3<>/dev/tcp/127.0.0.1/1883) 2>/dev/null; then
  systems=$(echo "$systems" | tr ' ' '\n' | grep -v '^mqtt$' | tr '\n' ' ')
  echo "# no MQTT broker on 127.0.0.1:1883, skipping mqtt" >&2
fi

quiesce() {
  pkill -f "$HERE/(srnp|mqtt|zmq|tcp)_bench (pong|sub)" 2>/dev/null
  sleep 0.5
}

latency() {  # binary payload
  quiesce
  "$HERE/$1" pong "$2" >/dev/null 2>&1 &
  local peer=$!
  sleep 1.5
  timeout 300 "$HERE/$1" ping "$2" "$LAT_ITERS" "$LAT_WARMUP" || echo "FAILED,$1,latency,$2"
  kill $peer 2>/dev/null
  wait $peer 2>/dev/null
}

throughput() {  # binary payload count
  quiesce
  BENCH_EXPECTED=$3 "$HERE/$1" sub "$2" &
  local peer=$!
  sleep 2
  timeout 300 "$HERE/$1" pub "$2" "$3" >/dev/null 2>&1
  wait $peer 2>/dev/null
}

echo "# latency:    RESULT,system,latency,payload,n,p50,p90,p99,p99.9,max,mean  (microseconds, ROUND TRIP)"
echo "# throughput: RESULT,system,throughput,payload,sent,recv,seconds,msgs_per_s,MB_per_s"

for payload in $PAYLOADS; do
  for s in $systems; do latency "${s}_bench" "$payload"; done
done

for payload in $PAYLOADS; do
  count=100000
  [ "$payload" -ge 65536 ] && count=5000
  for s in $systems; do throughput "${s}_bench" "$payload" "$count"; done
done

echo "# codec (in-process, nanoseconds per call)"
for payload in $PAYLOADS; do "$HERE/codec_micro" "$payload"; done
echo "# one client -> own-server hop in isolation, one way"
"$HERE/srnp_selfloop" 64 3000
