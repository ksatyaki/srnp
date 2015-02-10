/*
 * srnp_kernel.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#ifndef SRNP_KERNEL_H_
#define SRNP_KERNEL_H_

#include <Pair.h>

namespace srnp

{

/**
 * Set a pair in your pair-space.
 */
void setPair (const std::string& key, const std::string& value);

/**
 * Set a pair in another pair-space.
 */
void setRemotePair (int owner, std::string key, std::string value);


}

#endif /* SRNP_KERNEL_H_ */
