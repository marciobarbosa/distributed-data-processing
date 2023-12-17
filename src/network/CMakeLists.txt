file(GLOB SOURCES "*.cpp")

add_library(network_lib ${SOURCES})

target_include_directories(network_lib PUBLIC ${CMAKE_SOURCE_DIR}/include/network)
