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
	static boost::shared_ptr <Server> server_instance_;
	static boost::shared_ptr <Client> client_instance_;
	static boost::shared_ptr <PairQueue> pair_queue_;
	static boost::shared_ptr <boost::asio::io_service> io_service_;

public:

	/**
	 * Set a pair in your pair-space.
	 */
	friend void setPair (const std::string& key, const std::string& value);

	/**
	 * Set a pair in another pair-space.
	 */
	friend void setRemotePair (const int& owner, const std::string& key, const std::string& value);

	/**
	 * Initialize the 'kernel'.
	 */
	friend void initialize (int argn, char* args[], char* env[]);

	/**
	 * Shutdown the 'kernel'.
	 */
	friend void shutdown();

	/**
	 * Print the pairspace.
	 */
	friend void printPairSpace();

	/**
	 * Print the subscribed pairspace.
	 */
	friend void printSubscribedPairSpace();

	/**
	 * Register a callback on a tuple.
	 */
	friend void registerCallback(const int& owner, const std::string& key, Pair::CallbackFunction callback_fn);

	/**
	 * Cancel callback.
	 */
	friend void cancelCallback(const int& owner, const std::string& key);

	/**
	 * Return the owner_id.
	 */
	friend int getOwnerID();

	/**
	 * Register a Subscription.
	 */
	friend void registerSubscription(const int& owner, const std::string& key);

	/**
	 * Cancel Subscription.
	 */
	friend void cancelSubscription(const int& owner, const std::string& key);

};

/**
 * Set a pair in your pair-space.
 */
void setPair (const std::string& key, const std::string& value);

/**
 * Set a pair in another pair-space.
 */
void setRemotePair (const int& owner, const std::string& key, const std::string& value);

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
 * Print Pairspace.
 */
void printSubscribedPairSpace();

/**
 * Register a callback on a tuple.
 */
void registerCallback(const int& owner, const std::string& key, Pair::CallbackFunction callback_fn);

/**
 * Cancel callback.
 */
void cancelCallback(const int& owner, const std::string& key);

/**
 * Register a Subscription.
 */
void registerSubscription(const int& owner, const std::string& key);

/**
 * Cancel Subscription.
 */
void cancelSubscription(const int& owner, const std::string& key);

/**
 * Get owner Id.
 */
int getOwnerID();


}

#endif /* SRNP_KERNEL_H_ */
