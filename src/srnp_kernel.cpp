/*
 * srnp_kernel.c
 *
 *  Created on: Feb 15, 2015
 *      Author: ace
 */

#include <srnp_kernel.h>

namespace srnp
{

// Static variable declaration.
boost::shared_ptr <Server> KernelInstance::server_instance_;
boost::shared_ptr <Client> KernelInstance::client_instance_;
boost::shared_ptr <PairQueue> KernelInstance::pair_queue_;
boost::shared_ptr <boost::asio::io_service> KernelInstance::io_service_;

void initialize(int argn, char* args[], char* env[])
{
	if( getenv("SRNP_MASTER_IP") == NULL || getenv("SRNP_MASTER_PORT") == NULL)
	{
		SRNP_PRINT_INFO << "Missing environment variables \'SRNP_MASTER_IP\' and/or \'SRNP_MASTER_PORT\'. Please check.";
		shutdown();
	}
	std::string srnp_master_ip = getenv("SRNP_MASTER_IP");
	std::string srnp_master_port = getenv("SRNP_MASTER_PORT");
	int desired_owner_id = -1;

	for(int i = 1; i < argn; i++)
	{
		if(strcmp(args[i], "--owner-id") == 0)
		{
			if(i+1 < argn)
				desired_owner_id = atoi(args[i+1]);
			else
			{
				SRNP_PRINT_ERROR << "You have used the \'owner-id\' option but didn't specify one. That's criminal!";
				shutdown();
			}
		}

	}

	KernelInstance::io_service_ = boost::shared_ptr <boost::asio::io_service> (new boost::asio::io_service);
	KernelInstance::pair_queue_ = boost::shared_ptr <PairQueue> (new PairQueue);
	KernelInstance::server_instance_ = boost::shared_ptr <Server>
		(new Server(*KernelInstance::io_service_, srnp_master_ip, srnp_master_port, *KernelInstance::pair_queue_, desired_owner_id));

	std::stringstream server_port;
	server_port << KernelInstance::server_instance_->getPort();
	KernelInstance::client_instance_ = boost::shared_ptr <Client> (new Client(*KernelInstance::io_service_,
												"127.0.0.1", server_port.str(), *KernelInstance::pair_queue_));

}

void shutdown()
{
	SRNP_PRINT_INFO << "SRNP SHUTTING DOWN!";
	KernelInstance::io_service_.reset();
	KernelInstance::pair_queue_.reset();
	KernelInstance::server_instance_.reset();
	KernelInstance::client_instance_.reset();
	SRNP_PRINT_INFO << "Everthing is over!";
	exit(0);
}

void setPair(const std::string& key, const std::string& value)
{
	KernelInstance::client_instance_->setPair(key, value);
}

void setRemotePair(const int& owner, const std::string& key, const std::string& value)
{
	KernelInstance::client_instance_->setPair(owner, key, value);
}

void printPairSpace()
{
	KernelInstance::server_instance_->printPairSpace();
}

void printSubscribedPairSpace()
{
	KernelInstance::server_instance_->printSubscribedPairSpace();
}

void registerCallback(const int& owner, const std::string& key, Pair::CallbackFunction callback_fn)
{
	KernelInstance::client_instance_->registerCallback(owner, key, callback_fn);
}

void cancelCallback(const int& owner, const std::string& key)
{
	KernelInstance::client_instance_->cancelCallback(owner, key);
}

void registerSubscription(const int& owner, const std::string& key)
{
	KernelInstance::client_instance_->registerSubscription(owner, key);
}

void cancelSubscription(const int& owner, const std::string& key)
{
	KernelInstance::client_instance_->cancelSubscription(owner, key);
}

int getOwnerID ()
{
	return KernelInstance::server_instance_->owner();
}

}

