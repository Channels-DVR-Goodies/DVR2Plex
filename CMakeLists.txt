cmake_minimum_required(VERSION 3.10)

project(ChanDVR2Plex)
set( CMAKE_BUILD_TYPE DEBUG )

set( CMAKE_C_STANDARD 11 )
set( CMAKE_C_FLAGS    "-Wall -Wextra" )

include_directories(.)

file(GLOB SRCS "*.hash")
set(OUTFILES)
foreach(SRC ${SRCS})

    string(REGEX REPLACE "(.*).hash$" "\\1.h" OUTPUT_FILE_NAME ${SRC})

    add_custom_command(
      OUTPUT "${OUTPUT_FILE_NAME}"
      COMMAND hashstrings ${SRC}
      DEPENDS "${SRC}")

    set(OUTFILES ${OUTFILES} "${OUTPUT_FILE_NAME}")

endforeach(SRC)

add_custom_target(hashes ALL DEPENDS ${OUTFILES})

add_executable( ChanDVR2Plex chandvr2plex.c chandvr2plex.h dictionary.c dictionary.h)
target_link_libraries( ChanDVR2Plex "/usr/lib/x86_64-linux-gnu/libdl.so" )
