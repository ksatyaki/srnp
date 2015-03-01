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
 * Message sent from a component's Server to the MasterHub.
 */
struct IndicatePresence
{
	/**
	 * The port of the server.
	 * This is used to -
	 * (1) Seed the random number generator in the MasterHub.
	 * (2) Set communicate to others, on what port we should be contacted.
	 */
	std::string port;

	/**
	 * Tell the Master that we wish to force an Owner ID for ourselves.
	 */
	bool force_owner_id;

	/**
	 * Owner ID in case we wish to force.
	 */
	int owner_id;

	/**
	 * Default constructor to ensure that the by default we let the
	 * MasterHub choose our ID.
	 */
	IndicatePresence () : force_owner_id (false), owner_id (-1) { }

	/**
	 * Serializer for boost.
	 */
	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner_id;
		o_archive & force_owner_id;
		o_archive & port;
	}

};

/**
 * Information of one component.
 */
struct ComponentInfo
{
	int owner;
	std::string ip;
	std::string port;

	ComponentInfo () : owner (-1) { }

	/**
	 * Serializer for boost.
	 */
	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner;
		o_archive & ip;
		o_archive & port;
	}
};

/**
 * Message sent by master to a component's Server for updation of
 * components that the server is connected to.
 */
struct UpdateComponents
{
	enum Operation
	{
		ADD = 0,
		REMOVE,
		// Not used for now.
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

	/**
	 * Serializer for boost.
	 */
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
	 * Might be the same one requested.
	 */
	int owner;

	/**
	 * All the components registered on the MasterHub prior
	 * to the one receiving this message.
	 */
	std::vector <ComponentInfo> all_components;

	/**
	 * Serializer for boost.
	 */
	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner;
		o_archive & all_components;
	}

};



}
#endif /* MASTER_MESSAGES_H_ */
