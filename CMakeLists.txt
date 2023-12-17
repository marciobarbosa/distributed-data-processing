cmake_minimum_required(VERSION 3.16)
project(cbdp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
include("${CMAKE_SOURCE_DIR}/cmake/azure-storage-lite.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Werror -fsanitize=address")

find_package(CURL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/cmake/_deps/azure-storage-lite-src/include)

add_subdirectory(src/common)
add_subdirectory(src/coordinator)
add_subdirectory(src/network)
add_subdirectory(src/storage)
add_subdirectory(src/worker)

add_executable(coordinator coordinator.cpp)
target_link_libraries(coordinator coordinator_lib network_lib storage_lib common_lib CURL::libcurl azure-storage-lite)

add_executable(worker worker.cpp)
target_link_libraries(worker worker_lib network_lib storage_lib common_lib azure-storage-lite)
