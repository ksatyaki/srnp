// Raw TCP over loopback: the floor no middleware on this machine can beat.
// Length-prefixed frames, TCP_NODELAY on, one message in flight for latency.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "common.hpp"

namespace {

constexpr uint16_t kPort = 5571;

bool readAll(int fd, void* buf, size_t n) {
  auto* p = static_cast<char*>(buf);
  while (n) {
    const ssize_t r = ::read(fd, p, n);
    if (r <= 0) return false;
    p += r; n -= static_cast<size_t>(r);
  }
  return true;
}

bool writeAll(int fd, const void* buf, size_t n) {
  const auto* p = static_cast<const char*>(buf);
  while (n) {
    const ssize_t w = ::write(fd, p, n);
    if (w <= 0) return false;
    p += w; n -= static_cast<size_t>(w);
  }
  return true;
}

int listenSocket() {
  const int s = ::socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
  addr.sin_port = ::htons(kPort);
  if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
    std::perror("bind"); std::exit(1);
  }
  ::listen(s, 4);
  return s;
}

int accepted(int ls) {
  const int c = ::accept(ls, nullptr, nullptr);
  int one = 1;
  ::setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
  return c;
}

int connected() {
  const int s = ::socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
  addr.sin_port = ::htons(kPort);
  for (int i = 0; i < 200; ++i) {
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0) return s;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::perror("connect"); std::exit(1);
}

void runEcho(size_t payload) {
  const int ls = listenSocket();
  std::fprintf(stderr, "pong ready\n");
  const int c = accepted(ls);
  std::vector<char> buf(payload + 64);
  for (;;) {
    uint32_t len = 0;
    if (!readAll(c, &len, sizeof len)) break;
    if (len > buf.size()) buf.resize(len);
    if (!readAll(c, buf.data(), len)) break;
    if (!writeAll(c, &len, sizeof len)) break;
    if (!writeAll(c, buf.data(), len)) break;
  }
  ::close(c); ::close(ls);
}

void runPing(size_t payload, size_t iters, size_t warmup) {
  const int s = connected();
  std::vector<char> buf(payload + 64);
  std::vector<double> rtt;
  rtt.reserve(iters);
  for (size_t i = 0; i < warmup + iters; ++i) {
    const std::string p = makePayload(payload, i + 1);
    const uint32_t len = static_cast<uint32_t>(p.size());
    const auto t0 = Clock::now();
    writeAll(s, &len, sizeof len);
    writeAll(s, p.data(), p.size());
    uint32_t back = 0;
    readAll(s, &back, sizeof back);
    readAll(s, buf.data(), back);
    const auto t1 = Clock::now();
    if (i >= warmup) rtt.push_back(micros(t1 - t0));
  }
  ::close(s);
  reportLatency("raw-tcp", payload, std::move(rtt));
}

void runSink(size_t payload) {
  const int ls = listenSocket();
  std::fprintf(stderr, "sub ready\n");
  const int c = accepted(ls);
  std::vector<char> buf(payload + 64);
  size_t got = 0;
  Clock::time_point first{}, last{};
  for (;;) {
    uint32_t len = 0;
    if (!readAll(c, &len, sizeof len)) break;
    if (len > buf.size()) buf.resize(len);
    if (!readAll(c, buf.data(), len)) break;
    if (len == 3 && std::memcmp(buf.data(), "eof", 3) == 0) break;
    if (got++ == 0) first = Clock::now();
    last = Clock::now();
  }
  ::close(c); ::close(ls);
  const double seconds = got > 1 ? std::chrono::duration<double>(last - first).count() : 0;
  const char* expected = std::getenv("BENCH_EXPECTED");
  reportThroughput("raw-tcp", payload, expected ? std::strtoull(expected, nullptr, 10) : 0, got,
                   seconds);
}

void runSource(size_t payload, size_t n) {
  const int s = connected();
  for (size_t i = 0; i < n; ++i) {
    const std::string p = makePayload(payload, i);
    const uint32_t len = static_cast<uint32_t>(p.size());
    writeAll(s, &len, sizeof len);
    writeAll(s, p.data(), p.size());
  }
  const uint32_t len = 3;
  writeAll(s, &len, sizeof len);
  writeAll(s, "eof", 3);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  ::close(s);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <ping|pong|pub|sub> <payload> [n] [warmup]\n", argv[0]); return 2; }
  const std::string role = argv[1];
  const size_t payload = std::strtoull(argv[2], nullptr, 10);
  if (role == "pong") runEcho(payload);
  else if (role == "ping") runPing(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 10000,
                                   argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1000);
  else if (role == "sub") runSink(payload);
  else if (role == "pub") runSource(payload, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 100000);
  else return 2;
  return 0;
}
