cmake_minimum_required(VERSION 3.15)
project(LRZ-MPI VERSION 1.0.0 LANGUAGES  C)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

find_package(MPI REQUIRED)
if(NOT MPI_FOUND)
	message(FATAL_ERROR "MPI not found. Install OpenMPI, MPICH or IntelMPI")
endif()

# file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
# add_library(lmpi SHARED ${SOURCES})

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


target_include_directories(lmpi PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_include_directories(lmpi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/internal)


target_compile_features(lmpi PUBLIC c_std_11)
target_compile_options(lmpi PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter -pthread -fPIC)
target_link_libraries(lmpi PUBLIC MPI::MPI_C pthread m)

add_library(lmpi::lmpi ALIAS lmpi)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/)

install(TARGETS lmpi 
	EXPORT lmpi_target
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION include
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/lmpi.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

install(EXPORT lmpi_target
	FILE lmpi_target.cmake
	NAMESPACE lmpi::
	DESTINATION lib/cmake/lmpi
)
