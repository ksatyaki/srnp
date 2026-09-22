// Isolates the client -> own-server loopback hop: publish, and wait for the
// callback on our own pair. No second component involved.
#include <srnp/srnp_kernel.h>
#include <srnp/srnp_print.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "common.hpp"

std::mutex m; std::condition_variable cv; std::atomic<uint64_t> n{0};

int main(int argc, char* argv[]) {
  srnp::srnp_print_setup("error");
  const size_t payload = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 64;
  const size_t iters = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3000;
  srnp::initialize("127.0.0.1", "12321", 7, "selfloop");
  srnp::registerCallback(srnp::getOwnerID(), "self", [](const srnp::Pair::ConstPtr& p) {
    if (p->getValue().empty()) return;
    { std::lock_guard g(m); n.fetch_add(1); } cv.notify_one();
  });
  std::vector<double> rtt; rtt.reserve(iters);
  for (size_t i = 0; i < iters + 500; ++i) {
    const auto before = n.load();
    const auto t0 = Clock::now();
    (void)srnp::setPair("self", makePayload(payload, i));
    { std::unique_lock l(m); cv.wait_for(l, std::chrono::seconds(2), [&]{ return n.load() > before; }); }
    const auto t1 = Clock::now();
    if (i >= 500) rtt.push_back(micros(t1 - t0));
  }
  reportLatency("srnp-selfloop(one-way)", payload, std::move(rtt));
  srnp::shutdown();
}
