/*
  srnp_print.h - Many options for logging.
  
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

#ifndef SRNP_PRINT_H
#define SRNP_PRINT_H

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/core.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

#define SRNP_PRINT_TRACE BOOST_LOG_TRIVIAL(trace)
#define SRNP_PRINT_INFO BOOST_LOG_TRIVIAL(info)
#define SRNP_PRINT_DEBUG BOOST_LOG_TRIVIAL(debug)
#define SRNP_PRINT_WARNING BOOST_LOG_TRIVIAL(warning)
#define SRNP_PRINT_ERROR BOOST_LOG_TRIVIAL(error)
#define SRNP_PRINT_FATAL BOOST_LOG_TRIVIAL(fatal)

namespace srnp
{

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", boost::log::trivial::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void srnp_print_setup(const std::string& str_level = "");

}

#endif // SRNP_PRINT_H //
