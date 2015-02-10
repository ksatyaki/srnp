/*
 * HostInfo.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#ifndef MASTER_MESSAGES_H_
#define MASTER_MESSAGES_H_

#include <string>
#include <vector>
#include <utility>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>

namespace srnp
{

/**
 * Component is inspired from the PEIS component.
 */
struct ComponentInfo
{
	int owner;
	std::string ip;
	int port;

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner;
		o_archive & ip;
		o_archive & port;
	}
};

struct UpdateComponents
{
	enum Operation
	{
		ADD = 0,
		DELETE,
		MODIFY
	};

	/**
	 * The operation to do on the componenets list.
	 */
	Operation operation;

	/**
	 * The component to remove or add.
	 */
	ComponentInfo component;

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & operation;
		o_archive & component;
	}
};

/**
 * The message sent to a component from the master.
 * This contains the unique ID. (The PEIS id).
 */
struct MasterMessage
{
	/**
	 * An owner id for the requester to keep.
	 */
	int owner;

	std::vector <ComponentInfo> all_components;

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner;
		o_archive & all_components;
	}

};



}
#endif /* MASTER_MESSAGES_H_ */
