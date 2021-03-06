cmake_minimum_required( VERSION 3.5 )

project( hellosrv )

set( DIR_SOURCES src )
set( DIR_INCLUDES includes )
set( DIR_TESTS test )

set( DIR_C_PROCS ~/workspace/lib/c/c_procs )

include_directories( ${DIR_C_PROCS}/include )

set( SOURCES_C_PROCS
    ${DIR_C_PROCS}/src/strutils.c
    ${DIR_C_PROCS}/src/daemonutils.c
    ${DIR_C_PROCS}/src/netutils/netutils.c
)

set( LIBRARIES
	c_procs
)

# Be nice to visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Be nice and export compile commands by default, this is handy for clang-tidy and for other tools.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if ( DEFINED DIR_INCLUDES AND IS_DIRECTORY ${DIR_INCLUDES} )
    # Includes in separate directory
    include_directories( include )
endif()

#Scan dir for standart source files
aux_source_directory( ${DIR_SOURCES} SOURCES )

add_library( c_procs STATIC ${SOURCES_C_PROCS} )

# Add executable target
add_executable( hellosrv ${SOURCES} )

target_link_libraries( hellosrv ${LIBRARIES} )

if ( DEFINED DIR_TESTS )
    #set enable testing
    enable_testing()
    foreach ( dir IN LISTS DIR_TESTS )
        if ( IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dir} )
            message("test dir ${dir}")
            add_subdirectory( ${dir} )
        endif()
    endforeach()
endif()
