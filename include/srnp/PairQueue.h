/*
 * PairQueue.h
 *
 *  Created on: Feb 15, 2015
 *      Author: ace
 */

#ifndef PAIRQUEUE_H_
#define PAIRQUEUE_H_

#include <srnp/Pair.h>
#include <srnp/msgs/CommMessages.h>
#include <queue>
#include <boost/thread/mutex.hpp>

namespace srnp
{
struct PairQueue
{
	/**
	 * Queue used to send pairs from Client to Server.
	 */
	std::queue <Pair> pair_queue;

	/**
	 * ROS Users beware. This is a name chosen for convenience. It is not, in any way, related to ROS callback queues.
	 */
	std::queue < Pair::CallbackFunction > callback_queue;

	/**
	 * A queue to maintain the PairUpdate Messages.
	 */
	std::queue <Pair> pair_update_queue;

	/**
	 * A mutex to lock the pair queue.
	 */
	boost::mutex pair_queue_mutex;

	/**
	 * A mutex to lock the pair queue.
	 */
	boost::mutex pair_update_queue_mutex;

	/**
	 * A mutex to lock the callback queue.
	 */
	boost::mutex callback_queue_mutex;
};
}



#endif /* PAIRQUEUE_H_ */
