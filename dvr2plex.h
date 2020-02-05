//
// Created by Paul on 4/4/2019.
//

#ifndef DVR2PLEX_H
#define DVR2PLEX_H

#if CMAKE_BUILD_TYPE == Debug
#define DEBUG 1
#endif

typedef const char * string;

extern int gDebugLevel;
#define debugf( level, format, ... ) do { if (gDebugLevel >= level) fprintf( stderr, format, __VA_ARGS__ ); } while (0)

#endif // DVR2PLEX_H
