/*
  srnp_python.cpp - HIGHLY INCOMPLETE PYTHON WRAPPER
  
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


