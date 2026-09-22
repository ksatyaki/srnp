// MQTT (mosquitto broker, QoS 0) side of the sneak-peek benchmark.
#include <mosquitto.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "common.hpp"

namespace {

std::mutex mtx;
std::condition_variable cv;
std::atomic<uint64_t> arrived{0};
std::atomic<size_t> count{0};
std::atomic<bool> done{false};
Clock::time_point first_msg, last_msg;

const char* host() {
  const char* v = std::getenv("MQTT_HOST");
  return v ? v : "127.0.0.1";
}
int port() {
  const char* v = std::getenv("MQTT_PORT");
  return v ? std::atoi(v) : 1883;
}

struct Client {
  mosquitto* m = nullptr;
  explicit Client(const char* id) {
    m = mosquitto_new(id, true, nullptr);
    if (!m) { std::fprintf(stderr, "mosquitto_new failed\n"); std::exit(1); }
  }
  void connect() {
    const int rc = mosquitto_connect(m, host(), port(), 30);
    if (rc != MOSQ_ERR_SUCCESS) {
      std::fprintf(stderr, "mqtt connect failed: %s\n", mosquitto_strerror(rc));
      std::exit(1);
    }
    mosquitto_loop_start(m);
  }
  ~Client() {
    if (m) { mosquitto_loop_stop(m, true); mosquitto_destroy(m); }
  }
};

void runPong(size_t) {
  Client c("bench-pong");
  mosquitto_message_callback_set(c.m, [](mosquitto*, void* ud, const mosquitto_message* msg) {
    auto* self = static_cast<mosquitto*>(ud);
    mosquitto_publish(self, nullptr, "bench/pong", msg->payloadlen, msg->payload, 0, false);
  });
  mosquitto_user_data_set(c.m, c.m);
  c.connect();
  mosquitto_subscribe(c.m, nullptr, "bench/ping", 0);
  std::fprintf(stderr, "pong ready\n");
  for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
}

void runPing(size_t payload, size_t iters, size_t warmup) {
  Client c("bench-ping");
  mosquitto_message_callback_set(c.m, [](mosquitto*, void*, const mosquitto_message*) {
    { std::lock_guard lock(mtx); arrived.fetch_add(1, std::memory_order_relaxed); }
    cv.notify_one();
  });
  c.connect();
  mosquitto_subscribe(c.m, nullptr, "bench/pong", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  for (int attempt = 0; attempt < 200; ++attempt) {
    const uint64_t before = arrived.load();
    const std::string p = makePayload(payload, 0);
    mosquitto_publish(c.m, nullptr, "bench/ping", static_cast<int>(p.size()), p.data(), 0, false);
    std::unique_lock lock(mtx);
    if (cv.wait_for(lock, std::chrono::milliseconds(100), [&] { return arrived.load() > before; }))
      break;
  }
  if (arrived.load() == 0) { std::fprintf(stderr, "mqtt ping: no echo\n"); std::exit(1); }

  std::vector<double> rtt;
  rtt.reserve(iters);
  for (size_t i = 0; i < warmup + iters; ++i) {
    const std::string p = makePayload(payload, i + 1);
    const uint64_t before = arrived.load(std::memory_order_relaxed);
    const auto t0 = Clock::now();
    mosquitto_publish(c.m, nullptr, "bench/ping", static_cast<int>(p.size()), p.data(), 0, false);
    {
      std::unique_lock lock(mtx);
      if (!cv.wait_for(lock, std::chrono::seconds(5),
                       [&] { return arrived.load(std::memory_order_relaxed) > before; })) {
        std::fprintf(stderr, "mqtt ping: timed out at %zu\n", i);
        std::exit(1);
      }
    }
    const auto t1 = Clock::now();
    if (i >= warmup) rtt.push_back(micros(t1 - t0));
  }
  reportLatency("mqtt", payload, std::move(rtt));
}

void runSub(size_t payload) {
  Client c("bench-sub");
  mosquitto_message_callback_set(c.m, [](mosquitto*, void*, const mosquitto_message* msg) {
    if (std::strcmp(msg->topic, "bench/eof") == 0) { done.store(true); return; }
    if (count.fetch_add(1, std::memory_order_relaxed) == 0) first_msg = Clock::now();
    last_msg = Clock::now();
  });
  c.connect();
  mosquitto_subscribe(c.m, nullptr, "bench/data", 0);
  mosquitto_subscribe(c.m, nullptr, "bench/eof", 0);
  std::fprintf(stderr, "sub ready\n");

  size_t seen = 0, stable = 0;
  while (!done.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const size_t now = count.load();
    if (now != seen) { seen = now; stable = 0; }
    else if (seen > 0 && ++stable > 40) break;
  }
  const size_t got = count.load();
  const double seconds = got > 1 ? std::chrono::duration<double>(last_msg - first_msg).count() : 0;
  const char* expected = std::getenv("BENCH_EXPECTED");
  reportThroughput("mqtt", payload, expected ? std::strtoull(expected, nullptr, 10) : 0, got,
                   seconds);
}

void runPub(size_t payload, size_t n) {
  Client c("bench-pub");
  c.connect();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  for (size_t i = 0; i < n; ++i) {
    const std::string p = makePayload(payload, i);
    mosquitto_publish(c.m, nullptr, "bench/data", static_cast<int>(p.size()), p.data(), 0, false);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  mosquitto_publish(c.m, nullptr, "bench/eof", 4, "done", 0, false);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <ping|pong|pub|sub> <payload> [n] [warmup]\n", argv[0]); return 2; }
  mosquitto_lib_init();
  const std::string role = argv[1];
  const size_t payload = std::strtoull(argv[2], nullptr, 10);
  if (role == "pong") runPong(payload);
  else if (role == "ping") runPing(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 10000,
                                   argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1000);
  else if (role == "sub") runSub(payload);
  else if (role == "pub") runPub(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 100000);
  else return 2;
  mosquitto_lib_cleanup();
  return 0;
}
