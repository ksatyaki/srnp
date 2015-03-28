##SRNP - Simply Rewritten New PEIS


SRNP is what started as a hobby project.
Currently a few of the peiskernel functions are available. 
It's not even an alpha version now.


###Functions available

Below are some implemented functions and their counter-parts in PEIS


|S No.|PEIS Functions.               |SRNP Functions                 |
|----:|:-----------------------------|:------------------------------|
|    1|peiskmt_initialize            |`srnp::initialize`             |
|    2|peiskmt_subscribe             |`srnp::registerSubscription`   |
|    3|peiskmt_registerTupleCallback |`srnp::registerCallback`       |
|    4|peiskmt_setStringTuple        |`srnp::setPair`                |
|    5|peiskmt_setRemoteStringTuple  |REMOVED                        |



It is based on the PEIS Kernel by Mathias Broxwall.

https://github.com/mbrx/peisecology

									                           	   
###What is SRNP?

It's a reimplementation of the PEIS Kernel in C++ using Boost for a large part. 
It uses all principles from the PEIS Kernel - but it uses no source code from the
original PEIS written in C. The concepts were programmed as I understood them.
I did look at a few lines of the original code from time to time to get inspiration. 
Though, I must admit that I couldn't understand a lot of the HARD-CORE C stuff.
I hence implemented the protocol, etc., all by myself. 

I would love to have a PairView, a rewritten TupleView. I am not sure how soon, though.
#### PairView is available at https://github.com/ksatyaki/PairView

This README was created on 16/02/2015, when SRNP was 9 days old.
Last updated: 11 March, 2015

###Installation

####SRNP
Please create a separate directory for build.
Change to that directory and do:

	   cmake ../srnp
	   make
	   sudo make install

The default system installation path is used for installation.
This is /usr/local on GNU/Linux systems. You can choose a different directory by
changing the usual CMake options.
Don't forget to add the lib directory to LD_LIBRARY_PATH.

	  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

Depends:
		Boost version 1.54

I think this is the first version which has Boost.Log.
Surely works with 1.54 and 1.57, the latest.

####PairView
Please Create a separate directory for build.
Change to that directory and do:

	   cmake ../PairView
	   make

Currently PairView is a not entirely great. So it is not installed.
You can run PairView from this folder, however.

Alternatively, you could use the .pro file to build the package with qmake.

Depends:
		Qt4 or Qt5

Tested with both Qt4 and Qt5.



#### NOTE: THIS SOFTWARE IS LICENSED UNDER THE GNU GENERAL PUBLIC LICENSE v3. However, dependencies have different licences and are not included with this package.
