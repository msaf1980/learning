cmake_minimum_required( VERSION 3.5 )

# Options can be turned on and off by providing cmake with a -DOPTIONNAME=ON (or OFF) argument.
# For example, to turn off benchmarks, and use sanitize flags in a DEBUG build:
#    cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBENCH=OFF -DSANITIZE=ON

# Add test files with filenames <NAME>_test.cpp or <NAME>_test.c into test dir
# Add bench files with filenames <NAME>_bench.cpp or <NAME>_bench.c into test dir

# Override install prefix with CMAKE_INSTALL_PREFIX
# Override bin dir with BINDIR
# Override lib dir with LIBDIR

option(TEST      "Build tests" ON)
option(BENCH     "Build benchmarks" OFF)
option(ASAN      "Adds sanitize flags" OFF)
option(TSAN      "Adds thread sanitize flags" OFF)
option(DEBUGINFO   "Add debug info" ON)

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build: None Debug Release Coverage ASan ASanDbg MemSan MemSanDbg TSan TSanDbg"
    FORCE)

if(SANITIZE)
  set(TSANITIZE OFF)
endif()

if(BENCH)
  set(TEST ON)
endif()

set( DIR_SOURCES src )
set( DIR_INCLUDES include )
#set( DIR_TESTS test )
set( DIR_TESTS_TOOLS tools )
#set( DIR_BENCHS bench )
set( DIR_DEP dep )
set( DIR_SCRIPT cmake )
set( DIR_PRESCRIPT cmake_pre )

if (NOT DEFINED BINDIR)
set( BINDIR bin )
endif()
if (NOT DEFINED LIBDIR)
set( LIBDIR lib )
endif()

set( DIR_C_PROCS ../../../lib/c_procs )

set( SOURCES_C_PROCS
    ${DIR_C_PROCS}/src/strutils.c
    ${DIR_C_PROCS}/src/daemonutils.c
    ${DIR_C_PROCS}/src/netutils/netutils.c
)

include_directories( ${DIR_C_PROCS}/include )

set ( PROJECT echosrv-libevent-threaded )
set ( BINARY ${PROJECT} )

# Include cmake prescript from cmake dir
file(GLOB cmakeFiles ${DIR_PRESCRIPT}/*.cmake)
foreach(cmakeFile ${cmakeFiles})
    message("INCLUDE ${cmakeFile}")
    INCLUDE(${cmakeFile})
endforeach(cmakeFile)

project( ${PROJECT} )

file(GLOB cmakeFiles ${DIR_SCRIPT}/*.cmake)
foreach(cmakeFile ${cmakeFiles})
    message("INCLUDE ${cmakeFile}")
    INCLUDE(${cmakeFile})
endforeach(cmakeFile)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set ( LIBRARIES
    event
)

if ( DEFINED DIR_INCLUDES )
    # Includes in separate directory
    include_directories( ${DIR_INCLUDES} )
endif()

# Scan dir for standart source files
aux_source_directory( ${DIR_SOURCES} SOURCES )

# Append source file
#list(APPEND SOURCES
#	contrib/c_procs/src/strutils.c
#)

# Add sources from dir
#set( SOURCES
#    ${DIR_SOURCES}/main.cpp
#    ${DIR_SOURCES}/palindrom.cpp
#)

# Add executable target
add_executable( ${BINARY} ${SOURCES} ${SOURCES_C_PROCS} )
#target_include_directories( ${BINARY} ${DIR_INCLUDES} )
if(LIBRARIES)
    target_link_libraries ( ${BINARY} ${LIBRARIES} )
endif()

if(DEFINED ENV{DEVENV})
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

INSTALL( TARGETS ${BINARY} DESTINATION ${BINDIR} )

enable_testing()

if (TEST)

if ( DEFINED DIR_TESTS )
	#set enable testing
	foreach ( dir IN LISTS DIR_TESTS )
		message("add test ${dir}")
		add_subdirectory( ${dir} )
	endforeach()
endif()

if ( DEFINED DIR_TESTS_INTEGRATION )
	#set enable testing
	foreach ( dir IN LISTS DIR_TESTS_INTEGRATION )
		message("add integration test ${dir}")
		add_subdirectory( ${dir} )
	endforeach()
endif()

if ( DEFINED DIR_TESTS_TOOLS )
	#set enable testing
	foreach ( dir IN LISTS DIR_TESTS_TOOLS )
		message("add tools from ${dir}")
		file(GLOB cmakeFiles ${dir}/*.cmake)
		foreach(cmakeFile ${cmakeFiles})
			message("INCLUDE ${cmakeFile}")
			INCLUDE(${cmakeFile})
		endforeach(cmakeFile)
	endforeach()
endif()

endif() # END TEST


message(STATUS "")
message(STATUS "BUILD SUMMARY")
message(STATUS "  CMAKE_GENERATOR       : ${CMAKE_GENERATOR}")
message(STATUS "  Compiler ID           : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Build type            : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build tests           : ${TEST}")
message(STATUS "  Build benchmarks      : ${BENCH}")
message(STATUS "  Sanitize flags        : ${ASAN}")
message(STATUS "  Thread Sanitize flags : ${TSAN}")
message(STATUS "")
message(STATUS "  Install prefix : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Binary dir     : ${BINDIR}")
message(STATUS "  Lib dir        : ${LIBDIR}")
message(STATUS "")
message(STATUS "CMAKE_CXX_FLAGS            : ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_C_FLAGS              : ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS  : ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS     : ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "")
