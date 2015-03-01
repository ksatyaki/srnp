/*
 * srnp_kernel.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
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


}

#endif /* SRNP_KERNEL_H_ */
