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

void srnp_print_setup(const std::string& str_level = "")
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

#endif // SRNP_PRINT_H //
