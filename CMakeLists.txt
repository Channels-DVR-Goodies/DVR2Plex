cmake_minimum_required(VERSION 3.14)

project(ChanDVR2Plex)
set( CMAKE_BUILD_TYPE DEBUG )

set( CMAKE_C_STANDARD 11 )
set( CMAKE_C_FLAGS    "-Wall -Wextra" )

include_directories(.)

add_executable( ChanDVR2Plex chandvr2plex.c chandvr2plex.h )
