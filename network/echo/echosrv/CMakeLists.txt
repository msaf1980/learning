cmake_minimum_required( VERSION 3.5 )

set( DIR_SOURCES src )
set( DIR_INCLUDES include )
set( DIR_TESTS test )
set( DIR_DEP dep )

set( DIR_C_PROCS ../../../lib/c_procs )

set( SOURCES_C_PROCS
    ${DIR_C_PROCS}/src/strutils.c
    ${DIR_C_PROCS}/src/daemonutils.c
    ${DIR_C_PROCS}/src/netutils/netutils.c
)
set ( PROJECT echosrv )
set ( BINARY ${PROJECT} )

set( LIBRARIES
    c_procs
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project( ${PROJECT} )

if ( DEFINED DIR_INCLUDES AND IS_DIRECTORY ${DIR_INCLUDES} )
    # Includes in separate directory
    include_directories( ${DIR_INCLUDES} )
endif()

#Scan dir for standart source files
aux_source_directory( ${DIR_SOURCES} SOURCES )

include_directories( ${DIR_C_PROCS}/include )

add_library( c_procs STATIC ${SOURCES_C_PROCS} )

# Add executable target
add_executable( ${BINARY} ${SOURCES} )
#target_include_directories( ${BINARY} ${DIR_INCLUDES} )
target_link_libraries ( ${BINARY} ${LIBRARIES} )

enable_testing()

if ( DEFINED DIR_TESTS )
	#set enable testing
	foreach ( dir IN LISTS DIR_TESTS )
		if ( IS_DIRECTORY ${dir} )
			add_subdirectory( ${dir} )
		endif()
	endforeach()
endif()

if(DEFINED ENV{DEVENV})
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

