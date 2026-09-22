find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(srnp_unit_tests
  test_pair_space.cpp
  test_wire.cpp
  test_meta_parsing.cpp)
target_link_libraries(srnp_unit_tests PRIVATE srnp srnp_warnings GTest::gtest_main)
add_test(NAME srnp_unit_tests COMMAND srnp_unit_tests)

# Drives a real master and two real nodes over loopback.
add_executable(srnp_integration_tests test_integration.cpp)
target_link_libraries(srnp_integration_tests PRIVATE srnp srnp_warnings GTest::gtest_main)
target_compile_definitions(srnp_integration_tests PRIVATE
  SRNP_MASTER_BINARY="$<TARGET_FILE:srnp-master>")
add_test(NAME srnp_integration_tests COMMAND srnp_integration_tests)
set_tests_properties(srnp_integration_tests PROPERTIES TIMEOUT 120)

# The API reference is hand-written, so this is what stops it drifting.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME srnp_api_reference
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_api_reference.py)
else()
  message(STATUS "python3 not found; skipping the API reference check")
endif()
