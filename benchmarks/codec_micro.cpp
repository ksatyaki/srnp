// How much of the cost is the wire format? Batched so the clock calls do not
// show up in the measurement.
#include <srnp/Pair.h>
#include <srnp/wire.h>
#include <cstdio>
#include "common.hpp"

namespace srnp {
void encode(wire::Writer&, const Pair&);
void decode(wire::Reader&, Pair&);
}

int main(int argc, char* argv[]) {
  const size_t payload = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 64;
  const size_t iters = payload > 4096 ? 20000 : 200000;
  srnp::Pair p(1, "ping", std::string(payload, 'x'), srnp::Pair::Type::String);

  // Warm up the allocator so page faults do not land inside the timed loop.
  size_t frame_bytes = 0;
  for (size_t i = 0; i < 1000; ++i) {
    srnp::wire::Writer w; srnp::encode(w, p);
    auto b = std::move(w).take(); frame_bytes = b.size();
    srnp::wire::Reader r(b); srnp::Pair out; srnp::decode(r, out);
  }

  size_t sink = 0;
  const auto e0 = Clock::now();
  for (size_t i = 0; i < iters; ++i) {
    srnp::wire::Writer w;
    srnp::encode(w, p);
    sink += w.size();
  }
  const auto e1 = Clock::now();

  srnp::wire::Writer w; srnp::encode(w, p);
  const auto buf = std::move(w).take();
  const auto d0 = Clock::now();
  for (size_t i = 0; i < iters; ++i) {
    srnp::wire::Reader r(buf);
    srnp::Pair out;
    srnp::decode(r, out);
    sink += out.getValue().size();
  }
  const auto d1 = Clock::now();

  std::printf("CODEC,payload=%zu,frame_bytes=%zu,+16B_header,encode_ns=%.0f,decode_ns=%.0f%s\n",
              payload, frame_bytes,
              micros(e1 - e0) * 1000.0 / static_cast<double>(iters),
              micros(d1 - d0) * 1000.0 / static_cast<double>(iters),
              sink ? "" : "");
  return 0;
}
