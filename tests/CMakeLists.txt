cmake_minimum_required(VERSION 3.5)
project(CTest LANGUAGES C)
find_package(LMPI 2.0 REQUIRED)
add_executable(overlapTest ${CMAKE_CURRENT_SOURCE_DIR}/examples/main_mem_overlap.c)
target_link_libraries(overlapTest PRIVATE LMPI::LMPI)

