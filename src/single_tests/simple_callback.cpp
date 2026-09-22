// Subscribes to another component's pair and prints every update.

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

  // Subscribes to this key on every component, now and as they join.
  srnp::registerSubscription("value");
  srnp::registerCallback(srnp::kAnyOwner, "*", [](const srnp::Pair::ConstPtr& pair) {
    SRNP_INFO("update: {}", *pair);
  });

  std::this_thread::sleep_for(std::chrono::seconds(12));

  srnp::printPairSpace();
  srnp::shutdown();
  return 0;
}
