cmake_minimum_required(VERSION 3.10)

project(DVR2Plex)
set( CMAKE_BUILD_TYPE DEBUG )

set( CMAKE_C_STANDARD 11 )
set( CMAKE_C_FLAGS    "-Wall -Wextra" )

include_directories(.)

file(GLOB HASHES "*.hash")
set(OUTFILES)
foreach(HASH ${HASHES})

    string(REGEX REPLACE "(.*).hash$" "\\1.h" OUTPUT_FILE_NAME ${HASH})

    add_custom_command(
      OUTPUT "${OUTPUT_FILE_NAME}"
      COMMAND hashstrings ${HASH}
      DEPENDS "${HASH}")

    set(OUTFILES ${OUTFILES} "${OUTPUT_FILE_NAME}")

endforeach(HASH)

add_custom_target(hashes ALL DEPENDS ${OUTFILES})

add_executable( DVR2Plex dvr2plex.c dvr2plex.h dictionary.c dictionary.h)
target_link_libraries( DVR2Plex "/usr/lib/x86_64-linux-gnu/libdl.so" )
