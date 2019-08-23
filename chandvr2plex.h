//
// Created by Paul on 4/4/2019.
//

#ifndef CHANDVR2PLEX_H
#define CHANDVR2PLEX_H

#if CMAKE_BUILD_TYPE == Debug
#define DEBUG 1
#endif

extern int gDebugLevel;
#define debugf( level, format, ... ) do { if (gDebugLevel >= level) fprintf( stderr, format, __VA_ARGS__ ); } while (0)

#endif // CHANDVR2PLEX_H
