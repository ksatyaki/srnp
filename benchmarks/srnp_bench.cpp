// SRNP side of the sneak-peek benchmark.
//
//   srnp_bench pong  <payload>
//   srnp_bench ping  <payload> <iters> <warmup>
//   srnp_bench sub   <payload>
//   srnp_bench pub   <payload> <count>
//
// ping/pong measures round-trip latency with one message in flight.
// pub/sub measures one-way sustained throughput.

#include <srnp/srnp_kernel.h>
#include <srnp/srnp_print.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include "common.hpp"

namespace {

const char* masterIp() {
  const char* v = std::getenv("SRNP_MASTER_IP");
  return v ? v : "127.0.0.1";
}
const char* masterPort() {
  const char* v = std::getenv("SRNP_MASTER_PORT");
  return v ? v : "12321";
}

constexpr int kPingOwner = 1;
constexpr int kPongOwner = 2;

std::mutex mtx;
std::condition_variable cv;
std::atomic<uint64_t> arrived{0};

void runPong(size_t payload) {
  srnp::initialize(masterIp(), masterPort(), kPongOwner, "pong");
  srnp::registerSubscription(kPingOwner, "ping");
  srnp::registerCallback(kPingOwner, "ping", [payload](const srnp::Pair::ConstPtr& p) {
    if (p->getValue().empty()) return;  // placeholder from registration
    (void)srnp::setPair("pong", p->getValue().substr(0, payload));
  });
  std::fprintf(stderr, "pong ready\n");
  // Runs until the driver kills it.
  for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
}

void runPing(size_t payload, size_t iters, size_t warmup) {
  srnp::initialize(masterIp(), masterPort(), kPingOwner, "ping");
  srnp::registerSubscription(kPongOwner, "pong");
  srnp::registerCallback(kPongOwner, "pong", [](const srnp::Pair::ConstPtr& p) {
    if (p->getValue().empty()) return;
    {
      std::lock_guard lock(mtx);
      arrived.fetch_add(1, std::memory_order_relaxed);
    }
    cv.notify_one();
  });

  // Wait until the loop actually round-trips before timing anything.
  for (int attempt = 0; attempt < 200; ++attempt) {
    const uint64_t before = arrived.load();
    (void)srnp::setPair("ping", makePayload(payload, 0));
    std::unique_lock lock(mtx);
    if (cv.wait_for(lock, std::chrono::milliseconds(100),
                    [&] { return arrived.load() > before; }))
      break;
  }
  if (arrived.load() == 0) {
    std::fprintf(stderr, "srnp ping: no echo came back\n");
    std::exit(1);
  }

  std::vector<double> rtt;
  rtt.reserve(iters);
  for (size_t i = 0; i < warmup + iters; ++i) {
    const std::string payload_str = makePayload(payload, i + 1);
    const uint64_t before = arrived.load(std::memory_order_relaxed);
    const auto t0 = Clock::now();
    (void)srnp::setPair("ping", payload_str);
    {
      std::unique_lock lock(mtx);
      if (!cv.wait_for(lock, std::chrono::seconds(5),
                       [&] { return arrived.load(std::memory_order_relaxed) > before; })) {
        std::fprintf(stderr, "srnp ping: timed out at iteration %zu\n", i);
        std::exit(1);
      }
    }
    const auto t1 = Clock::now();
    if (i >= warmup) rtt.push_back(micros(t1 - t0));
  }

  reportLatency("srnp", payload, std::move(rtt));
  srnp::shutdown();
}

void runSub(size_t payload) {
  srnp::initialize(masterIp(), masterPort(), kPongOwner, "sub");
  std::atomic<size_t> count{0};
  Clock::time_point first{};
  std::atomic<Clock::time_point> last_msg{Clock::time_point{}};
  std::atomic<bool> done{false};

  srnp::registerSubscription(kPingOwner, "data");
  srnp::registerSubscription(kPingOwner, "eof");
  srnp::registerCallback(kPingOwner, "data", [&](const srnp::Pair::ConstPtr& p) {
    if (p->getValue().empty()) return;
    const auto now = Clock::now();
    if (count.fetch_add(1, std::memory_order_relaxed) == 0) first = now;
    last_msg.store(now, std::memory_order_relaxed);
  });
  srnp::registerCallback(kPingOwner, "eof", [&](const srnp::Pair::ConstPtr& p) {
    if (p->getValue().empty()) return;
    done.store(true);
  });

  std::fprintf(stderr, "sub ready\n");
  size_t stable = 0, seen = 0;
  while (!done.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const size_t now = count.load();
    if (now != seen) {
      seen = now;
      stable = 0;
    } else if (seen > 0 && ++stable > 100) {
      break;  // publisher's eof never arrived; the stream has gone quiet
    }
  }

  const size_t got = count.load();
  const Clock::time_point last = last_msg.load();
  const double seconds =
      got > 1 ? std::chrono::duration<double>(last - first).count() : 0.0;
  const char* expected = std::getenv("BENCH_EXPECTED");
  reportThroughput("srnp", payload, expected ? std::strtoull(expected, nullptr, 10) : 0, got,
                   seconds);
  srnp::shutdown();
}

void runPub(size_t payload, size_t count) {
  srnp::initialize(masterIp(), masterPort(), kPingOwner, "pub");
  // Give the subscriber's subscription time to land.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  for (size_t i = 0; i < count; ++i) (void)srnp::setPair("data", makePayload(payload, i));
  std::this_thread::sleep_for(std::chrono::seconds(8));
  (void)srnp::setPair("eof", "done");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  srnp::shutdown();
}

}  // namespace

int main(int argc, char* argv[]) {
  srnp::srnp_print_setup("error");
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <ping|pong|pub|sub> <payload> [n] [warmup]\n", argv[0]);
    return 2;
  }
  const std::string role = argv[1];
  const size_t payload = std::strtoull(argv[2], nullptr, 10);
  try {
    if (role == "pong") runPong(payload);
    else if (role == "ping") runPing(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 10000,
                                    argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1000);
    else if (role == "sub") runSub(payload);
    else if (role == "pub") runPub(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 100000);
    else { std::fprintf(stderr, "unknown role %s\n", role.c_str()); return 2; }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "srnp_bench %s failed: %s\n", role.c_str(), e.what());
    return 1;
  }
  return 0;
}
