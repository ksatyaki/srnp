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

void srnp_print_setup(boost::log::trivial::severity_level level)
{
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
