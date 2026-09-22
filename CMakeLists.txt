cmake_minimum_required (VERSION 3.16)
project (srnp VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package (Boost REQUIRED COMPONENTS system)

set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

add_library(srnp_warnings INTERFACE)
target_compile_options(srnp_warnings INTERFACE -Wall -Wextra)

# Sanitizers are how the threading and lifetime work gets verified.
# Use -DSRNP_SANITIZE=address,undefined or -DSRNP_SANITIZE=thread.
set(SRNP_SANITIZE "" CACHE STRING "Sanitizers to build with (e.g. address,undefined or thread)")
if(SRNP_SANITIZE)
  message(STATUS "Building with sanitizers: ${SRNP_SANITIZE}")
  target_compile_options(srnp_warnings INTERFACE
    -fsanitize=${SRNP_SANITIZE} -fno-omit-frame-pointer -g)
  target_link_options(srnp_warnings INTERFACE -fsanitize=${SRNP_SANITIZE})
endif()

add_library (srnp SHARED
  src/Pair.cpp
  src/PairSpace.cpp
  src/codec.cpp
  src/wire.cpp
  src/session.cpp
  src/master_hub.cpp
  src/srnp_print.cpp
  src/server.cpp
  src/client.cpp
  src/srnp_kernel.cpp
  src/meta_pair_callback.cpp)

target_include_directories(srnp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries (srnp PUBLIC Boost::system PRIVATE srnp_warnings)
set_target_properties(srnp PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 0)

# Master
add_executable (srnp-master src/master_main.cpp)
target_link_libraries (srnp-master PRIVATE srnp srnp_warnings)

# Demos
add_executable (simple src/single_tests/simple.cpp)
target_link_libraries (simple PRIVATE srnp srnp_warnings)

add_executable (simple_callback src/single_tests/simple_callback.cpp)
target_link_libraries (simple_callback PRIVATE srnp srnp_warnings)

add_executable (simple2 src/single_tests/simple2.cpp)
target_link_libraries (simple2 PRIVATE srnp srnp_warnings)

add_executable (testGetTupleIndirectly src/single_tests/testGetTupleIndirectly.cc)
target_link_libraries (testGetTupleIndirectly PRIVATE srnp srnp_warnings)

enable_testing()
add_subdirectory(tests)

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
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake")

include(CPackSettings)
