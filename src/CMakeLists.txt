# CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.
cmake_minimum_required (VERSION 2.8.11)
project (neuro)

set(Boost_USE_STATIC_LIBS "ON")
set(BOOST_ROOT "/usr/local/boost_1_61_0")
find_package(Boost COMPONENTS system filesystem date_time REQUIRED)

set(CMAKE_CXX_FLAGS "-std=c++14")
set(CMAKE_CXX_FLAGS "-W -Wall ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-isystem ${Boost_INCLUDE_DIRS} ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-ggdb -O0 ${CMAKE_CXX_FLAGS}")
# set(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")

add_executable(core core.cpp)

add_executable(parse parse.cpp)

add_executable(parse2 parse2.cpp)

add_library(next next.cpp)

add_executable(next_test next_test.cpp)
target_link_libraries(next_test next)

add_executable(mycurl curl.cpp)
target_link_libraries(mycurl curl ${Boost_SYSTEM_LIBRARY})

add_executable(curl_asio curl_asio.cpp)
target_link_libraries(curl_asio curl ${Boost_SYSTEM_LIBRARY})

add_executable(multi_thread multi_thread.cpp)
target_link_libraries(multi_thread next curl pthread config++ ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})