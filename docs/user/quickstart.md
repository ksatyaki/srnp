# Quickstart

Two components: one publishes a temperature, the other watches it. By the end you
will have seen a pair appear, change, and be deleted.

Build srnp first — see [Building](building.md). Everything below assumes
`./build/bin` holds the binaries and `./build/lib` the library.

## 1. Start the master

The master hands out owner ids and tells components about each other. Nothing
works without it.

```bash
./build/bin/srnp-master 12321
```

```
[17:57:26.867] (info): master listening on port 12321
SRNP master ready on port 12321
```

Leave it running and open two more terminals. In each of them:

```bash
export SRNP_MASTER_IP=127.0.0.1
export SRNP_MASTER_PORT=12321
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH
```

## 2. The publisher

`publisher.cpp`:

```cpp
#include <srnp/srnp_kernel.h>

#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char* argv[]) {
  try {
    srnp::initialize(argc, argv);
  } catch (const srnp::InitError& e) {
    std::fprintf(stderr, "could not start: %s\n", e.what());
    return 1;
  }

  std::printf("publisher is owner %d\n", srnp::getOwnerID());

  for (int i = 0; i < 5; ++i) {
    (void)srnp::setPair("temperature", std::to_string(20 + i));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  (void)srnp::removePair("temperature");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  srnp::shutdown();
  return 0;
}
```

## 3. The subscriber

A callback is registered against one exact owner and key, so the subscriber has
to know which component publishes the pair. The publisher is started below with
`--owner-id 1000` to make that id fixed instead of assigned at random.

`subscriber.cpp`:

```cpp
#include <srnp/srnp_kernel.h>

#include <chrono>
#include <cstdio>
#include <thread>

/// The id the publisher asks for with --owner-id.
constexpr int kPublisher = 1000;

int main(int argc, char* argv[]) {
  try {
    srnp::initialize(argc, argv);
  } catch (const srnp::InitError& e) {
    std::fprintf(stderr, "could not start: %s\n", e.what());
    return 1;
  }

  srnp::registerSubscription(kPublisher, "temperature");
  srnp::registerCallback(kPublisher, "temperature", [](const srnp::Pair::ConstPtr& pair) {
    std::printf("temperature is now %s\n", pair->getValue().c_str());
  });

  std::this_thread::sleep_for(std::chrono::seconds(5));

  if (srnp::getPair(kPublisher, "temperature")) std::printf("the pair is still there\n");
  else std::printf("the pair is gone\n");

  srnp::shutdown();
  return 0;
}
```

## 4. Build and run them

```bash
g++ -std=c++20 -I include publisher.cpp -L build/lib -lsrnp -o publisher
g++ -std=c++20 -I include subscriber.cpp -L build/lib -lsrnp -o subscriber
```

Start the subscriber first, so it is registered before anything is published:

```bash
./subscriber
```

Then, in the other terminal:

```bash
./publisher --owner-id 1000
```

The subscriber prints:

```
[17:58:00.602] (info): registered with the master as owner 8375
[17:58:00.605] (info): node "./subscriber" started with owner id 8375
temperature is now 20
temperature is now 21
temperature is now 22
temperature is now 23
temperature is now 24
the pair is gone
```

Its own owner id is whatever the master assigned; only the publisher's was
pinned. The five callbacks are the five values, and `the pair is gone` is
`removePair` having reached the subscriber.

## What to read next

- [Concepts](concepts.md) for what an owner, a pair and a subscription actually
  are.
- [API reference](api-reference.md) for everything else you can call.
