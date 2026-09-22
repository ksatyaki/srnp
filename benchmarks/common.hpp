// Shared helpers for the middleware sneak-peek benchmarks.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

inline double micros(Clock::duration d) {
  return std::chrono::duration<double, std::micro>(d).count();
}

inline double percentile(std::vector<double> sorted, double p) {
  if (sorted.empty()) return 0.0;
  const double idx = p * (static_cast<double>(sorted.size()) - 1.0);
  const size_t lo = static_cast<size_t>(std::floor(idx));
  const size_t hi = static_cast<size_t>(std::ceil(idx));
  const double frac = idx - static_cast<double>(lo);
  return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

// Round-trip latencies in microseconds -> one CSV line.
inline void reportLatency(const char* system, size_t payload, std::vector<double> rtt) {
  std::sort(rtt.begin(), rtt.end());
  double sum = 0.0;
  for (double v : rtt) sum += v;
  const double mean = rtt.empty() ? 0.0 : sum / static_cast<double>(rtt.size());
  std::printf("RESULT,%s,latency,%zu,%zu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", system, payload,
              rtt.size(), percentile(rtt, 0.50), percentile(rtt, 0.90), percentile(rtt, 0.99),
              percentile(rtt, 0.999), rtt.empty() ? 0.0 : rtt.back(), mean);
  std::fflush(stdout);
}

inline void reportThroughput(const char* system, size_t payload, size_t sent, size_t recv,
                             double seconds) {
  const double rate = seconds > 0 ? static_cast<double>(recv) / seconds : 0.0;
  const double mbps = rate * static_cast<double>(payload) / (1024.0 * 1024.0);
  std::printf("RESULT,%s,throughput,%zu,%zu,%zu,%.4f,%.0f,%.2f\n", system, payload, sent, recv,
              seconds, rate, mbps);
  std::fflush(stdout);
}

// A payload of the requested size whose first bytes carry the sequence number,
// so no layer can mistake two consecutive messages for the same value.
inline std::string makePayload(size_t size, uint64_t seq) {
  std::string s(size, 'x');
  const size_t n = std::min<size_t>(size, 16);
  char buf[24];
  std::snprintf(buf, sizeof buf, "%016llu", static_cast<unsigned long long>(seq));
  std::memcpy(s.data(), buf, n);
  return s;
}
