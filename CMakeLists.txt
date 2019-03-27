cmake_minimum_required (VERSION 3.0 FATAL_ERROR)
set(CMP0048 new)
project (srnp VERSION 0.1.0 LANGUAGES CXX)

find_package (Boost
  COMPONENTS
  thread
  system
  serialization
  date_time
  log
  log_setup
)

set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")


if(Boost_LOG_FOUND)
  add_definitions(-DWITH_BOOST_LOG)
  ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
  message("Boost log found. We will now build SRNP_PRINT to use boost log.")
else(Boost_LOG_FOUND)
  message("Boost.Log not found. SRNP_PRINT messages are simply std::couts. ")
endif()

# BOOST PYTHON
include_directories(/usr/include/python2.7)

# Project Stuff
include_directories (include ${Boost_INCLUDE_DIRS})

add_library (srnp SHARED
  src/Pair.cpp
  src/PairSpace.cpp
  src/srnp_print.cpp
  src/server.cpp
  src/client.cpp
  src/srnp_kernel.cpp
  src/meta_pair_callback.cpp)
target_link_libraries (srnp ${Boost_LIBRARIES})

# Master
add_executable (srnp-master src/master_hub.cpp)
target_link_libraries (srnp-master srnp ${Boost_LIBRARIES})

# Python Wrapper
# add_library (srnpy SHARED src/srnp_python.cpp)
# target_link_libraries (srnpy srnp_kernel ${Boost_LIBRARIES})

# set_target_properties(srnpy PROPERTIES PREFIX "")

# Test
add_executable (simple src/single_tests/simple.cpp)
target_link_libraries (simple srnp)

add_executable (simple_callback src/single_tests/simple_callback.cpp)
target_link_libraries (simple_callback srnp)

add_executable (simple2 src/single_tests/simple2.cpp)
target_link_libraries (simple2 srnp)

add_executable (testGetTupleIndirectly src/single_tests/testGetTupleIndirectly.cc)
target_link_libraries (testGetTupleIndirectly srnp)

install (DIRECTORY include/srnp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT srnp)
install (TARGETS srnp DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT srnp)
install (TARGETS srnp-master DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT srnp)

include(CMakePackageConfigHelpers)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
configure_package_config_file(srnpConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/srnpConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/srnp/cmake
    PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
    NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/srnpConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/srnpConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/srnpConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/srnp/cmake
    COMPONENT srnp)

########### Add uninstall target ###############
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake") 


include(CPackSettings)
### ROS ###
if (EXISTS "package.xml")
  # install catkin package.xml
  MESSAGE("Installing package.xml")
  install(FILES package.xml DESTINATION "share/srnp")
endif()
