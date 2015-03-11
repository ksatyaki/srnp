/*
  PairQueue.h - Used to communicate a pair without putting it on the
  socket.
  
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
