/*
  master_main.cpp - The srnp-master executable.

  Copyright (C) 2015  Chittaranjan Srinivas Swaminathan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <srnp/master_hub.h>
#include <srnp/srnp_print.h>

#include <boost/asio/signal_set.hpp>

#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
  srnp::srnp_print_setup(std::getenv("SRNP_LOG_LEVEL") != nullptr ? std::getenv("SRNP_LOG_LEVEL")
                                                                  : "info");

  unsigned short port = 12321;
  if (const char* from_env = std::getenv("SRNP_MASTER_PORT"))
    port = static_cast<unsigned short>(std::atoi(from_env));
  if (argc > 1) port = static_cast<unsigned short>(std::atoi(argv[1]));

  try {
    boost::asio::io_context io;
    srnp::MasterHub master(io, port);

    // Print the bound port so a caller that asked for 0 can find it.
    std::printf("SRNP master ready on port %u\n", master.port());
    std::fflush(stdout);

    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&io](auto, auto) { io.stop(); });

    io.run();
  } catch (const std::exception& e) {
    SRNP_FATAL("master could not start: {}", e.what());
    return 1;
  }
  return 0;
}
