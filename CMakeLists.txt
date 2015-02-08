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

add_executable (simple src/simple.cpp)
target_link_libraries (simple srnp_server ${Boost_LIBRARIES})

add_executable (test_client src/test_client.cpp)
target_link_libraries (test_client ${Boost_LIBRARIES})

