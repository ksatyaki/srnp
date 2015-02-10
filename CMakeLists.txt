cmake_minimum_required (VERSION 2.8 FATAL_ERROR)

project (srnp)

find_package (Boost REQUIRED 
    COMPONENTS
    thread
    system
    serialization
)

include_directories (include ${Boost_INCLUDE_DIRS})

add_library (srnp_server SHARED src/server.cpp)
target_link_libraries (srnp_server ${Boost_LIBRARIES})

add_library (srnp_client SHARED src/client.cpp)
target_link_libraries (srnp_client ${Boost_LIBRARIES})

add_executable (srnp_master src/master.cpp)
target_link_libraries (srnp_master ${Boost_LIBRARIES})



add_executable (simple src/simple.cpp)
target_link_libraries (simple srnp_server srnp_client ${Boost_LIBRARIES})

add_executable (test_pair_space src/single_tests/test_pair_space.cpp)
target_link_libraries (test_pair_space ${Boost_LIBRARIES})



