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
)

include_directories (include ${Boost_INCLUDE_DIRS})

# Pair Space
add_library (pair_space SHARED src/PairSpace.cpp)
target_link_libraries (pair_space ${Boost_LIBRARIES})

# Server
add_library (srnp_server SHARED src/server.cpp)
target_link_libraries (srnp_server pair_space ${Boost_LIBRARIES})

# Client
add_library (srnp_client SHARED src/client.cpp)
target_link_libraries (srnp_client pair_space ${Boost_LIBRARIES})

# Master
add_executable (master_hub src/master_hub.cpp)
target_link_libraries (master_hub ${Boost_LIBRARIES})

# Wrapper for all
add_library (srnp_kernel SHARED src/srnp_kernel.cpp)
target_link_libraries (srnp_kernel srnp_server srnp_client pair_space ${Boost_LIBRARIES})

# Test
add_executable (simple src/simple.cpp)
target_link_libraries (simple srnp_kernel)


