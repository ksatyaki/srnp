// ZeroMQ PUB/SUB side of the sneak-peek benchmark. Brokerless, so it is the
// closest architectural match to srnp.
#include <zmq.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "common.hpp"

namespace {

void* ctx = nullptr;

void* makeSocket(int type, const char* endpoint, bool bind) {
  void* s = zmq_socket(ctx, type);
  int hwm = 1000000;
  zmq_setsockopt(s, ZMQ_SNDHWM, &hwm, sizeof hwm);
  zmq_setsockopt(s, ZMQ_RCVHWM, &hwm, sizeof hwm);
  int linger = 0;
  zmq_setsockopt(s, ZMQ_LINGER, &linger, sizeof linger);
  const int rc = bind ? zmq_bind(s, endpoint) : zmq_connect(s, endpoint);
  if (rc != 0) { std::fprintf(stderr, "zmq %s %s failed: %s\n", bind ? "bind" : "connect",
                              endpoint, zmq_strerror(zmq_errno())); std::exit(1); }
  if (type == ZMQ_SUB) zmq_setsockopt(s, ZMQ_SUBSCRIBE, "", 0);
  return s;
}

const char* kPingEp = "tcp://127.0.0.1:5561";
const char* kPongEp = "tcp://127.0.0.1:5562";

void runPong(size_t payload) {
  void* sub = makeSocket(ZMQ_SUB, kPingEp, false);
  void* pub = makeSocket(ZMQ_PUB, kPongEp, true);
  std::vector<char> buf(payload + 64);
  std::fprintf(stderr, "pong ready\n");
  for (;;) {
    const int n = zmq_recv(sub, buf.data(), buf.size(), 0);
    if (n < 0) continue;
    zmq_send(pub, buf.data(), static_cast<size_t>(n), 0);
  }
}

void runPing(size_t payload, size_t iters, size_t warmup) {
  void* pub = makeSocket(ZMQ_PUB, kPingEp, true);
  void* sub = makeSocket(ZMQ_SUB, kPongEp, false);
  std::vector<char> buf(payload + 64);

  // PUB/SUB drops anything sent before the subscription is wired up, so
  // keep sending until one comes back.
  int timeout = 100;
  zmq_setsockopt(sub, ZMQ_RCVTIMEO, &timeout, sizeof timeout);
  bool up = false;
  for (int attempt = 0; attempt < 200 && !up; ++attempt) {
    const std::string p = makePayload(payload, 0);
    zmq_send(pub, p.data(), p.size(), 0);
    if (zmq_recv(sub, buf.data(), buf.size(), 0) >= 0) up = true;
  }
  if (!up) { std::fprintf(stderr, "zmq ping: no echo\n"); std::exit(1); }
  timeout = 5000;
  zmq_setsockopt(sub, ZMQ_RCVTIMEO, &timeout, sizeof timeout);
  // Drain anything the handshake left queued.
  int drain_to = 0;
  zmq_setsockopt(sub, ZMQ_RCVTIMEO, &drain_to, sizeof drain_to);
  while (zmq_recv(sub, buf.data(), buf.size(), 0) >= 0) {}
  drain_to = 5000;
  zmq_setsockopt(sub, ZMQ_RCVTIMEO, &drain_to, sizeof drain_to);

  std::vector<double> rtt;
  rtt.reserve(iters);
  for (size_t i = 0; i < warmup + iters; ++i) {
    const std::string p = makePayload(payload, i + 1);
    const auto t0 = Clock::now();
    zmq_send(pub, p.data(), p.size(), 0);
    if (zmq_recv(sub, buf.data(), buf.size(), 0) < 0) {
      std::fprintf(stderr, "zmq ping: timed out at %zu\n", i); std::exit(1);
    }
    const auto t1 = Clock::now();
    if (i >= warmup) rtt.push_back(micros(t1 - t0));
  }
  reportLatency("zeromq", payload, std::move(rtt));
}

void runSub(size_t payload) {
  void* sub = makeSocket(ZMQ_SUB, kPingEp, false);
  std::vector<char> buf(payload + 64);
  std::fprintf(stderr, "sub ready\n");
  int timeout = 3000;
  zmq_setsockopt(sub, ZMQ_RCVTIMEO, &timeout, sizeof timeout);

  size_t got = 0;
  Clock::time_point first{}, last{};
  for (;;) {
    const int n = zmq_recv(sub, buf.data(), buf.size(), 0);
    if (n < 0) break;  // quiet for 3s: the run is over
    if (n == 3 && std::memcmp(buf.data(), "eof", 3) == 0) break;
    if (got++ == 0) first = Clock::now();
    last = Clock::now();
  }
  const double seconds = got > 1 ? std::chrono::duration<double>(last - first).count() : 0;
  const char* expected = std::getenv("BENCH_EXPECTED");
  reportThroughput("zeromq", payload, expected ? std::strtoull(expected, nullptr, 10) : 0, got,
                   seconds);
}

void runPub(size_t payload, size_t n) {
  void* pub = makeSocket(ZMQ_PUB, kPingEp, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(800));
  for (size_t i = 0; i < n; ++i) {
    const std::string p = makePayload(payload, i);
    zmq_send(pub, p.data(), p.size(), 0);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  zmq_send(pub, "eof", 3, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <ping|pong|pub|sub> <payload> [n] [warmup]\n", argv[0]); return 2; }
  ctx = zmq_ctx_new();
  const std::string role = argv[1];
  const size_t payload = std::strtoull(argv[2], nullptr, 10);
  if (role == "pong") runPong(payload);
  else if (role == "ping") runPing(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 10000,
                                   argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1000);
  else if (role == "sub") runSub(payload);
  else if (role == "pub") runPub(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 100000);
  else return 2;
  return 0;
}
