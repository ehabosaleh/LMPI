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
set_source_files_properties(src/lmpi_progress.c PROPERTIES COMPILE_OPTIONS "-Wno-restrict")

add_library(LMPI STATIC
  src/lmpi_comm.c
  src/lmpi_globals.c
  src/lmpi_init.c
  src/lmpi_memory.c
  src/lmpi_progress.c
  src/lmpi_queue.c
  src/lmpi_sendrecv.c
  src/lmpi_wait.c
)


target_include_directories(LMPI PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_include_directories(LMPI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/internal)


target_compile_features(LMPI PUBLIC c_std_11)
target_compile_options(LMPI PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter -pthread -fPIC)
target_link_libraries(LMPI PUBLIC MPI::MPI_C pthread m)

add_library(LMPI::LMPI ALIAS LMPI)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/)

install(TARGETS LMPI 
	EXPORT lmpi_target
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION include
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/lmpi.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

install(EXPORT lmpi_target
	FILE LMPITargets.cmake
	NAMESPACE LMPI::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lmpi
)


write_basic_package_version_file(
	LMPIConfigVersion.cmake
    	VERSION ${PROJECT_VERSION}
    	COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
	cmake/LMPIConfig.cmake.in
    	LMPIConfig.cmake
    	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lmpi
)

install(FILES 
	${CMAKE_CURRENT_BINARY_DIR}/LMPIConfig.cmake
    	${CMAKE_CURRENT_BINARY_DIR}/LMPIConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lmpi
)
