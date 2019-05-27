//
// Created by Paul on 4/4/2019.
//

#ifndef CHANDVR2PLEX_CHANDVR2PLEX_H
#define CHANDVR2PLEX_CHANDVR2PLEX_H

#if CMAKE_BUILD_TYPE == Debug
#define DEBUG 1
#endif

#ifndef DEBUG
#define debugf( format, ... )
#else
#define debugf( format, ... ) fprintf( stderr, format, __VA_ARGS__ )
#endif

#endif //CHANDVR2PLEX_CHANDVR2PLEX_H
