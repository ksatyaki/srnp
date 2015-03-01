/*
 * srnp_python.cpp
 *
 *  Created on: Mar 1, 2015
 *      Author: ace
 */

#include <srnp/srnp_kernel.h>
#include <srnp/srnp_print.h>
#include <boost/python.hpp>


BOOST_PYTHON_MODULE(srnpy)
{
    boost::python::def ("initialize", srnp::initialize_py);
    boost::python::def ("print_setup", srnp::srnp_print_setup);
    boost::python::def ("shutdown", srnp::shutdown);
    boost::python::def ("set_pair", srnp::setPair);
    boost::python::def ("print_pair_space", srnp::printPairSpace);
    boost::python::def ("register_callback", srnp::registerCallback);
    boost::python::def ("cancel_callback", srnp::cancelCallback);
    boost::python::def ("subscribe", srnp::registerSubscription);
    boost::python::def ("cancel_subscription", srnp::cancelSubscription);
    boost::python::def ("get_owner_id", srnp::getOwnerID);
}


