file(GLOB SOURCES "*.cpp")

add_library(coordinator_lib ${SOURCES})
target_include_directories(coordinator_lib PUBLIC ${CMAKE_SOURCE_DIR}/include/coordinator)
