# SRNP - Simply Rewritten New PEIS

SRNP is a blackboard middleware. Components publish key/value pairs, subscribe
to each other's pairs, and get callbacks when those change. It started as a
hobby reimplementation of the PEIS Kernel by Mathias Broxvall
(https://github.com/mbrx/peisecology) — the concepts, not the code.

## Functions available

Below are some implemented functions and their counter-parts in PEIS.

|S No.|PEIS Functions.               |SRNP Functions                 |
|----:|:-----------------------------|:------------------------------|
|    1|peiskmt_initialize            |`srnp::initialize`             |
|    2|peiskmt_subscribe             |`srnp::registerSubscription`   |
|    3|peiskmt_registerTupleCallback |`srnp::registerCallback`       |
|    4|peiskmt_setStringTuple        |`srnp::setPair`                |
|    5|peiskmt_setRemoteStringTuple  |`srnp::setRemotePair`          |

## How it works

`srnp-master` hands out owner ids and tells every component about the others.
It carries no pairs. Components then talk to each other directly: each one runs
a server holding its pair space, and a client that pushes updates to whoever
subscribed.

## Building

Needs a C++20 compiler and Boost (for Asio). GoogleTest is used for the tests,
and downloaded automatically if it isn't already installed.

    cmake -S . -B build
    cmake --build build
    sudo cmake --install build

Add the install prefix's lib directory to `LD_LIBRARY_PATH` if it isn't already:

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

## Running

Start the master, then point components at it:

    ./build/bin/srnp-master 12321

    export SRNP_MASTER_IP=127.0.0.1
    export SRNP_MASTER_PORT=12321
    ./build/bin/simple2

`SRNP_LOG_LEVEL` sets how much the master prints (trace, debug, info, warning,
error, fatal, off). A component calls `srnp::srnp_print_setup` for the same.

## Tests

    ctest --test-dir build --output-on-failure

The unit tests cover the pair space and the wire codec. The integration tests
run a real master and several real components over loopback.

Build with sanitizers to check the threading and lifetime behaviour:

    cmake -S . -B build-asan -DSRNP_SANITIZE=address,undefined
    cmake -S . -B build-tsan -DSRNP_SANITIZE=thread

## Changes in 0.2.0

- Requires C++20 and a current Boost. The old Boost-1.54-era code no longer
  compiled at all: `io_service`, `asio::strand` and the rest are long gone.
- **The wire format changed and is not compatible with 0.1.** Messages are now
  a fixed 16 byte binary header plus a length-prefixed binary payload, instead
  of Boost text archives. Rebuild every component, including PairView.
- `initialize` throws `srnp::InitError` instead of calling `exit`, and
  `shutdown` returns instead of ending the process.
- The Python wrapper (Boost.Python against Python 2.7, incomplete and not
  built) and the ROS 1 catkin files are gone.

## Related

PairView, a viewer for the pair space, lives at
https://github.com/ksatyaki/PairView — it needs a rebuild against this version.

## Licence

This software is licensed under the GNU General Public License v3.
Dependencies have different licences and are not included with this package.
