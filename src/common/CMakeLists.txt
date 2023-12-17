file(GLOB SOURCES "*.cpp")

add_library(common_lib ${SOURCES})
target_include_directories(common_lib PUBLIC ${CMAKE_SOURCE_DIR}/include/common)
