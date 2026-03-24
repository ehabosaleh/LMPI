cmake_minimum_required(VERSION 3.15)
project(LRZ-MPI C)

find_package(MPI REQUIRED)

add_library(lmpi STATIC
  src/lmpi_comm.c
  src/lmpi_globals.c
  src/lmpi_init.c
  src/lmpi_memory.c
  src/lmpi_progress.c
  src/lmpi_queue.c
  src/lmpi_sendrecv.c
  src/lmpi_wait.c
)

target_include_directories(lmpi
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
)

target_compile_features(lmpi PUBLIC c_std_11)
target_compile_options(lmpi PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter -pthread -fPIC)
target_link_libraries(lmpi PUBLIC MPI::MPI_C pthread m)

# Optional SHARED lib
add_library(lmpi_shared SHARED $<TARGET_OBJECTS:lmpi>)
set_target_properties(lmpi_shared PROPERTIES OUTPUT_NAME lmpi)
target_link_libraries(lmpi_shared PUBLIC MPI::MPI_C pthread)

add_executable(simple.out examples/main.c)
target_link_libraries(simple.out lmpi)

