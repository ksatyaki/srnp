// Reads a pair through a meta-pair that names it.

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

  (void)srnp::setPair("target", "the real value");
  (void)srnp::setMetaPair(srnp::getOwnerID(), "pointer", srnp::getOwnerID(), "target");

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  if (const auto pair = srnp::getPairIndirectly(srnp::getOwnerID(), "pointer"))
    SRNP_INFO("followed the meta-pair to: {}", pair->getValue());
  else
    SRNP_ERROR("the meta-pair did not resolve");

  srnp::shutdown();
  return 0;
}
