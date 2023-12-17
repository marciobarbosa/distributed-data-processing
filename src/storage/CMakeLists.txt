file(GLOB SOURCES "*.cpp")

add_library(storage_lib ${SOURCES})

target_include_directories(storage_lib PUBLIC ${CMAKE_SOURCE_DIR}/include/storage)
