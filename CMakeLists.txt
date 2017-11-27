cmake_minimum_required(VERSION 3.5)
project(proxy)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

find_package(Boost COMPONENTS program_options system coroutine regex REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")

set(SOURCE_FILES main.cpp server.cpp server.h connection.cpp connection.h request.cpp request.h)
add_executable(mproxy ${SOURCE_FILES})
target_link_libraries(mproxy ${Boost_LIBRARIES} pthread ${OPENSSL_LIBRARIES})