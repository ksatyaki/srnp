cmake_minimum_required (VERSION 2.8 FATAL_ERROR)

project (srnp)

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
find_package (Boost REQUIRED 
    COMPONENTS
    thread
    system
    serialization
    date_time
    log
    log_setup
    python
)

# BOOST PYTHON
include_directories(/usr/include/python2.7)

# Project Stuff
include_directories (include ${Boost_INCLUDE_DIRS})

# Pair - only for cout support
add_library (srnp_pair SHARED src/Pair.cpp)

# Pair Space
add_library (srnp_pair_space SHARED src/PairSpace.cpp)
target_link_libraries (srnp_pair_space ${Boost_LIBRARIES})

# SRNP Print
add_library (srnp_print SHARED src/srnp_print.cpp)
target_link_libraries (srnp_print ${Boost_LIBRARIES})

# Server
add_library (srnp_server SHARED src/server.cpp)
target_link_libraries (srnp_server srnp_pair srnp_pair_space ${Boost_LIBRARIES})

# Client
add_library (srnp_client SHARED src/client.cpp)
target_link_libraries (srnp_client srnp_pair srnp_pair_space ${Boost_LIBRARIES})

# Master
add_executable (srnp-master src/master_hub.cpp)
target_link_libraries (srnp-master srnp_print ${Boost_LIBRARIES})

# Wrapper for all
add_library (srnp_kernel SHARED src/srnp_kernel.cpp)
target_link_libraries (srnp_kernel srnp_pair srnp_server srnp_client srnp_pair_space srnp_print ${Boost_LIBRARIES})

# Python Wrapper
# add_library (srnpy SHARED src/srnp_python.cpp)
# target_link_libraries (srnpy srnp_kernel ${Boost_LIBRARIES})

# set_target_properties(srnpy PROPERTIES PREFIX "")

# Test
add_executable (simple src/single_tests/simple.cpp)
target_link_libraries (simple srnp_kernel)

add_executable (simple2 src/single_tests/simple2.cpp)
target_link_libraries (simple2 srnp_kernel)

install (DIRECTORY include/srnp DESTINATION "include")
install (TARGETS srnp_print srnp_pair srnp_pair_space srnp_server srnp_client srnp_kernel DESTINATION "lib")
install (TARGETS srnp-master DESTINATION "bin")


########### Add uninstall target ###############
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake") 
