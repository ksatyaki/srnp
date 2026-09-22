// Publishes under a fixed key so another component can subscribe to it.

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

  (void)srnp::initMetaPair(srnp::getOwnerID(), "simple");
  (void)srnp::setMetaPair(srnp::getOwnerID(), "simple", srnp::getOwnerID(), "value");

  for (int i = 0; i < 20; ++i) {
    (void)srnp::setPair("value", std::to_string(i * i));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  srnp::printPairSpace();
  srnp::shutdown();
  return 0;
}
