/*
  srnp_kernel.h
  A Wrapper for all the SRNP tools.
  
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

#ifndef SRNP_KERNEL_H_
#define SRNP_KERNEL_H_

#include <srnp/client.h>
#include <srnp/server.h>

namespace srnp

{

	class KernelInstance
	{
	private:

	public:
		static boost::shared_ptr <Server> server_instance_;
		static boost::shared_ptr <Client> client_instance_;
		static boost::shared_ptr <PairQueue> pair_queue_;
		static boost::shared_ptr <boost::asio::io_service> io_service_;

		static bool ok;
	
	};

/**
 * Set a pair in your pair-space.
 */
	void setPair (const std::string& key, const std::string& value);

/**
 * A function to initialize SRNP from python.
 */
	void initialize_py (const std::string& ip, const std::string& port);

/**
 * Initialize the 'kernel'.
 */
	void initialize (int argn, char* args[], char* env[]);

/**
 * Shutdown the 'kernel'.
 */
	void shutdown();

/**
 * Print Pairspace.
 */
	void printPairSpace();

/**
 * Register a callback on a tuple.
 */
	void registerCallback(const std::string& key, Pair::CallbackFunction callback_fn);

/**
 * Cancel callback.
 */
	void cancelCallback(const std::string& key);

/**
 * Register a Subscription.
 */
	void registerSubscription(const std::string& key);

/**
 * Cancel Subscription.
 */
	void cancelSubscription(const std::string& key);

/**
 * Get owner Id.
 */
	int getOwnerID();

	/** 
	 * Test if the kernel is running.
	 * @return True if it is, false otherwise.
	 */
	bool ok();


}

#endif /* SRNP_KERNEL_H_ */
