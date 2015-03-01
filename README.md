##SRNP - Simply Rewritten New PEIS


SRNP is what started as a hobby project.
Currently a few of the peiskernel functions are available. 
It's not even an alpha version now.


###Functions available


	 (i) 	peiskmt_initialize 				=== srnp::initialize
	(ii) 	peiskmt_subscribe 				=== srnp::registerSubscription
       (iii) 	peiskmt_registerTupleCallback 	=== srnp::registerCallback
	(iv) 	peiskmt_setStringTuple 			=== srnp::setPair
	 (v) 	peiskmt_setRemoteStringTuple 	=== srnp::setRemotePair


It is entirely based on the PEIS Kernel by Mathias Broxwall.

https://github.com/mbrx/peisecology


###What is SRNP?

It's a complete recreation of the PEIS Kernel in C++ using Boost for a large part.
It uses all principles from the PEIS Kernel - but it uses no source code from the
original PEIS written in C. The concepts were programmed as I understood them.
I did look at a few lines of the original code from time to time to get inspiration. 
Though, I must admit that I couldn't understand a lot of the HARD-CORE C stuff.
I hence implemented the protocol, etc., all by myself. 

I would love to have a PairView, a rewritten TupleView. I am not sure how soon, though.

This README was last updated on 16/02/2015, when SRNP was 9 days old.