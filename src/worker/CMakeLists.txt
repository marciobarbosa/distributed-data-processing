file(GLOB SOURCES "*.cpp")

add_library(worker_lib ${SOURCES})

target_include_directories(worker_lib PUBLIC ${CMAKE_SOURCE_DIR}/include/worker)
