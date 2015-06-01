#include <srnp/meta_pair_callback.hpp>

namespace srnp {
/**
 * A map to hold the details of registered meta-pairs.
 * The meta-pair might point to a real pair, or might be null.
 * This map connects the {owner, meta-pair} to the content of the pair.
 */
std::map <PairKey, std::string> all_metas;

/**
 * A map to hold the details of all meta-pair callbacks registered.
 */
std::map <PairKey, boost::shared_ptr<MetaCallbackInfo> > meta_map;

std::vector<std::string> extractStrings(const char p[])
{
	char *copyOfString = new char[strlen(p) + 1];
	strcpy(copyOfString, p);

	std::vector<std::string> _values;

	char *pch;
	int cmd_args = 0;

	pch = strtok (copyOfString," )(");
	while (pch != NULL)
	{
		cmd_args++;
		_values.push_back(pch);
		pch = strtok (NULL, " )(");
	}

	delete copyOfString;
	return _values;
}

void __registerCallback(std::vector <std::string> values, MetaCallbackInfo* meta_callback_info) {

	if(values.size() != 3) {
		printf("WARNING: A meta-callback registration was attempted on a pair that was not meta.\n");
		meta_callback_info->hasACallback = false;
		return;
	}
	else {
		if(values[0].compare("META") != 0) {
			printf("WARNING: A meta-callback registration was attempted on a pair that was not meta.\n");
			meta_callback_info->hasACallback = false;
			return;
		}
		else {
			//printf("Adding callback to:\nName: %s, Owner: %d...\n", values[2].c_str(), atoi(values[1].c_str()));
			meta_callback_info->callback_handle = registerCallback(atoi(values[1].c_str()), values[2], meta_callback_info->fn);
			meta_callback_info->hasACallback = true;
			return;
		}
	}
}

void __registerSubscription(std::vector <std::string> values, MetaCallbackInfo* meta_callback_info) {

	if(values.size() != 3) {
		printf("WARNING: A meta-subscription registration was attempted on a pair that was not meta.\n");
		meta_callback_info->hasASubscription = false;
		return;
	}
	else {
		if(values[0].compare("META") != 0) {
			printf("WARNING: A meta-callback subscription was attempted on a pair that was not meta.\n");
			meta_callback_info->hasASubscription = false;
			return;
		}
		else {
			//printf("Adding callback to:\nName: %s, Owner: %d...\n", values[2].c_str(), atoi(values[1].c_str()));
			meta_callback_info->subscriber_handle = registerSubscription(atoi(values[1].c_str()),values[2]);
			meta_callback_info->hasASubscription = true;
			return;
		}
	}
}

void metaCallback(const Pair::ConstPtr& metapair, MetaCallbackInfo* meta_callback_info) {
	//printf("Saw a meta-pair change.\n");

	//printf("Name: %s\nOwner: %d\n", buffer, metapair->owner);

	PairKey this_one(metapair->getOwner(), metapair->getKey());
	std::map<PairKey, std::string>::iterator it = all_metas.find(this_one);

	if(it != all_metas.end()) {
		if(it->second.compare(metapair->getValue()) == 0) {
			//printf("Callback exists, Not adding anything.\n");
			return;
		}
		else if(metapair->getValue().compare("(META -1 NULL)") == 0) {
			//printf("Callback deleted.\n");

			if(meta_map.find(this_one) == meta_map.end()) {
				printf("\n\n!!! Something is terribly wrong !!!\n\n\n");
			}

			meta_map[this_one]->hasASubscription = false;
			meta_map[this_one]->hasACallback = false;
			// Unregister old callback.
			cancelCallback(meta_map[this_one]->callback_handle);
			cancelSubscription(meta_map[this_one]->subscriber_handle);
			all_metas.erase(it);
			
			return;
		}
		else {
			all_metas[this_one] = metapair->getValue();
			//printf("Callback changed!\n");

			// UNREGISTER PREVIOUS.
			cancelCallback(meta_map[this_one]->callback_handle);
			cancelSubscription(meta_map[this_one]->subscriber_handle);

			// REGISTER NEW.
			__registerCallback(extractStrings(metapair->getValue().c_str()), meta_callback_info);
			__registerSubscription(extractStrings(metapair->getValue().c_str()), meta_callback_info);
		}
	}

	else {

		if(std::string(metapair->getValue()).compare("(META -1 NULL)") == 0) {
			//printf("Nothing to do. NULL link callback.\n");
			return;
		}

		all_metas[this_one] = metapair->getValue();
		//printf("Callback added!\n");

		// REGISTER NEW.
		__registerCallback(extractStrings(metapair->getValue().c_str()), meta_callback_info);
		__registerSubscription(extractStrings(metapair->getValue().c_str()), meta_callback_info);
	}
}

void registerMetaCallback(const int& meta_owner_id, const std::string& meta_pair_key, const Pair::CallbackFunction& cb) {

	PairKey this_pair_key(meta_owner_id, meta_pair_key);
	
	if(meta_map.find(this_pair_key) != meta_map.end()) {
		//printf("\nA meta-subscription to %s already exists. Nothing doing...\n", meta_pair_key);
		return;
	}

	meta_map[this_pair_key] = boost::shared_ptr<MetaCallbackInfo> (new MetaCallbackInfo (cb));

	meta_map[this_pair_key]->meta_subscriber_handle = registerSubscription(meta_owner_id, meta_pair_key);
	meta_map[this_pair_key]->meta_callback_handle = registerCallback(meta_owner_id, meta_pair_key, boost::bind(metaCallback, _1, meta_map[this_pair_key].get()));
	//printf("Callback registerd... But may not happen till the meta is linked... \n");
}

void cancelMetaCallback(int meta_owner_id, const std::string& meta_pair_key) {
 
	PairKey this_pair_key(meta_owner_id, meta_pair_key);	
	std::map <PairKey, boost::shared_ptr<MetaCallbackInfo> >::iterator it = meta_map.find(this_pair_key);	

	if(it != meta_map.end()) {
		//printf("\nA meta-subscription to %s exists. Deleting...\n", meta_pair_key);
		if(meta_map[this_pair_key]->hasACallback) {
			//printf("\nA meta-callback to %s also exists. Deleting...\n", meta_pair_key);
		    cancelCallback(meta_map[this_pair_key]->callback_handle);
			if(all_metas.find(this_pair_key) != all_metas.end())
				all_metas.erase(all_metas.find(this_pair_key));
			else
				printf("!!!TERRIBLE ERROR!!!\n");
        }

		if(meta_map[this_pair_key]->hasASubscription) {
			cancelSubscription(meta_map[this_pair_key]->subscriber_handle);
		}
		//printf("Deleted callback\n");
		cancelSubscription(meta_map[this_pair_key]->meta_subscriber_handle);
	    cancelCallback(meta_map[this_pair_key]->meta_callback_handle);
		meta_map.erase(it);
        //printf("DELETED!\n");
		return;
	}
	else {
		//printf("No subscription exists.\n");
	}
}

}
