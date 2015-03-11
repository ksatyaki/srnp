/*
  srnp_print.cpp
  
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

#include <srnp/srnp_print.h>

namespace srnp
{

void srnp_print_setup(const std::string& str_level)
{
	boost::log::trivial::severity_level level;

	if(str_level.compare ("debug") == 0 || str_level.compare ("DEBUG") == 0)
		level = boost::log::trivial::debug;
	else if(str_level.compare ("fatal") == 0 || str_level.compare ("FATAL") == 0)
		level = boost::log::trivial::fatal;
	else if(str_level.compare ("error") == 0 || str_level.compare ("ERROR") == 0)
		level = boost::log::trivial::error;
	else if(str_level.compare ("trace") == 0 || str_level.compare ("TRACE") == 0)
		level = boost::log::trivial::trace;
	else if(str_level.compare ("warning") == 0 || str_level.compare ("WARNING") == 0)
		level = boost::log::trivial::warning;
	else
		level = boost::log::trivial::info;

	boost::log::add_common_attributes();
	boost::shared_ptr<boost::log::core> core = boost::log::core::get();

	// setup console log
	boost::log::add_console_log (
	    std::clog,
		boost::log::keywords::filter = severity >= level,
		boost::log::keywords::format = (
	    		boost::log::expressions::stream << "[SRNP | "<< boost::log::expressions::format_date_time(timestamp, "%H:%M:%S %d-%m-%Y") <<"] (" << severity << "): " << boost::log::expressions::smessage
	    )
	);
}

}
