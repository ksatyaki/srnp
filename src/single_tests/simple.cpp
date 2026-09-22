// Publishes a pair and watches another component's pair through a meta-pair.

#include <srnp/meta_pair_callback.hpp>
#include <srnp/srnp_kernel.h>
#include <srnp/srnp_print.h>

#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char* argv[]) {
  srnp::srnp_print_setup("debug");

  try {
    srnp::initialize(argc, argv);
  } catch (const srnp::InitError& e) {
    std::fprintf(stderr, "could not start: %s\n", e.what());
    return 1;
  }

  srnp::registerMetaCallback(1, "simple", [](const srnp::Pair::ConstPtr& pair) {
    SRNP_INFO("meta target changed: {} = {}", pair->getKey(), pair->getValue());
  });

  for (int i = 0; i < 20; ++i) {
    (void)srnp::setPair("counter", std::to_string(i));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  srnp::cancelMetaCallback(1, "simple");
  srnp::shutdown();
  return 0;
}
