#ifndef SRNP_META_PAIR_CALLBACK_HPP_
#define SRNP_META_PAIR_CALLBACK_HPP_

#include <map>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <boost/shared_ptr.hpp>

#include <srnp/srnp_kernel.h>

namespace srnp {
	
	typedef std::pair <int, std::string> PairKey;

	struct MetaCallbackInfo {

		bool hasACallback;
		bool hasASubscription;
		SubscriptionHandle meta_subscriber_handle;
		SubscriptionHandle subscriber_handle;
		CallbackHandle meta_callback_handle;
		CallbackHandle callback_handle;
		Pair::CallbackFunction fn;

		MetaCallbackInfo (Pair::CallbackFunction _fn_) :
			fn(_fn_), hasACallback(false), hasASubscription(false) {}
	};

    /** 
     * A function that is called when the meta-pair changes. Ideally
     * this should be part of the original metaCallback in the PEIS kernel.
     * 
     * @param metapair The meta pair that has changed.
     * @param userdata Any user data.
     */
	void metaCallback(const Pair::ConstPtr& metapair, MetaCallbackInfo* meta_callback_info);

    /** 
      * A function to register a callback on the meta-pair. 
      * 
      * @param meta_owner_id The owner id of the component that owns the
      *                      meta-pair.
      * @param meta_pair_key The key of the meta-pair.
      * @param fn The callback that should be invoked.
	  * @param userdata The userdata to be passed.
	  */
	void registerMetaCallback(const int& meta_owner_id, const std::string& meta_pair_key, const Pair::CallbackFunction& cb);

	void registerMetaSubscription(const int& meta_owner_id, const std::string& meta_pair_key);

	void cancelMetaSubscription(const int& meta_owner_id, const std::string& meta_pair_key);

     /** 
      * A function to 'un-register', i.e., cancel the meta-callback on a owner, key pair.
      * 
      * @param meta_owner_id The owner id of the meta pair.
      * @param meta_pair_key The key of the meta pair.
      */
	void cancelMetaCallback(const int& meta_owner_id, const std::string& meta_pair_key);
}

#endif


