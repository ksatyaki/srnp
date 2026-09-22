# The comparison harnesses. Off unless -DSRNP_BENCHMARKS=ON, so a normal
# build and CI never pay for them.

add_executable (srnp_bench srnp_bench.cpp)
target_link_libraries (srnp_bench PRIVATE srnp)
target_include_directories (srnp_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable (srnp_selfloop srnp_selfloop.cpp)
target_link_libraries (srnp_selfloop PRIVATE srnp)
target_include_directories (srnp_selfloop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable (codec_micro codec_micro.cpp)
target_link_libraries (codec_micro PRIVATE srnp)
target_include_directories (codec_micro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# The floor. No dependency beyond libc.
add_executable (tcp_bench tcp_bench.cpp)
target_include_directories (tcp_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
find_package (Threads REQUIRED)
target_link_libraries (tcp_bench PRIVATE Threads::Threads)

# The two comparators are optional: without them the rest still builds, you
# just have fewer columns in the results.
find_path (MOSQUITTO_INCLUDE_DIR mosquitto.h)
find_library (MOSQUITTO_LIBRARY mosquitto)
if (MOSQUITTO_INCLUDE_DIR AND MOSQUITTO_LIBRARY)
  add_executable (mqtt_bench mqtt_bench.cpp)
  target_include_directories (mqtt_bench PRIVATE ${MOSQUITTO_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries (mqtt_bench PRIVATE ${MOSQUITTO_LIBRARY} Threads::Threads)
else ()
  message (STATUS "benchmarks: libmosquitto not found, skipping mqtt_bench")
endif ()

find_path (ZMQ_INCLUDE_DIR zmq.h)
find_library (ZMQ_LIBRARY zmq)
if (ZMQ_INCLUDE_DIR AND ZMQ_LIBRARY)
  add_executable (zmq_bench zmq_bench.cpp)
  target_include_directories (zmq_bench PRIVATE ${ZMQ_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries (zmq_bench PRIVATE ${ZMQ_LIBRARY} Threads::Threads)
else ()
  message (STATUS "benchmarks: libzmq not found, skipping zmq_bench")
endif ()

# Next to the binaries it drives, not in the benchmarks build dir.
configure_file (run_bench.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run_bench.sh COPYONLY)
