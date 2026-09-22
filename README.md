# SRNP - Simply Rewritten New PEIS

SRNP is a blackboard middleware. Components publish key/value pairs, subscribe
to each other's pairs, and get callbacks when those change. It started as a
hobby reimplementation of the PEIS Kernel by Mathias Broxvall
(https://github.com/mbrx/peisecology) — the concepts, not the code.

`srnp-master` hands out owner ids and tells every component about the others. It
carries no pairs: components talk to each other directly.

## Documentation

https://SRNP_DOCS_URL_PLACEHOLDER

Until that is deployed, the same pages are in `docs/` — start at
[`docs/index.md`](docs/index.md).

## Dependencies

A C++20 compiler, Boost (for Asio), and CMake 3.16 or newer. GoogleTest is used
for the tests and downloaded automatically if it isn't already installed.

    sudo apt-get install cmake g++ libboost-dev libboost-system-dev libgtest-dev

## Building

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

## Tests

    ctest --test-dir build --output-on-failure

## Licence

This software is licensed under the GNU General Public License v3.
Dependencies have different licences and are not included with this package.
