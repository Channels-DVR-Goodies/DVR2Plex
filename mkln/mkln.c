//
// Created by paul on 6/26/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <linux/limits.h>

char * gInvokedAs;

#if 0
const char * typeStrMap[] =
{
    [S_IFBLK]  = "block device",
    [S_IFCHR]  = "char device",
    [S_IFDIR]  = "directory",
    [S_IFIFO]  = "pipe",
    [S_IFLNK]  = "symlink",
    [S_IFREG]  = "file",
    [S_IFSOCK] = "socket"
};
#endif

int usage( void )
{
    fprintf( stderr, "Usage: %s <original> <target>\n", gInvokedAs );
    return -2;
}

int printErrno( char * format, ...)
{
    int errorNumber = errno;
    char msgBuf[2048];
    va_list args;

    va_start( args, format );

    vsnprintf( msgBuf, sizeof(msgBuf), format, args );
    fprintf( stderr, "%s: %s (%d: %s)\n", gInvokedAs, msgBuf, errorNumber, strerror(errorNumber) );

    va_end( args );

    return errorNumber;
}

int createPath( char * path, int mode )
{
    char * parent;
    struct stat here;

    if ( stat( path, &here ) == -1 )
    {
        switch (errno)
        {
        case ENOENT:
            parent = strdup(path);
            if ( createPath( dirname( parent ), mode ) == 0 )
            {
                if ( mkdir( path, mode ) == -1 )
                {
                    return printErrno( "unable to create directory \"%s\"", path );
                }
            }
            free( parent );
            break;

        default:
            printErrno("stat returned ");
            break;
        }
    }
    return 0;
}

int main( int argc, char *argv[] )
{
    struct stat originalInfo, targetInfo;
    char * original;
    char * target;

    gInvokedAs = strdup(basename(argv[0]) );

    if ( argc != 3 )
    {
        return usage();
    }

    original = strdup( argv[1] );
    target   = strdup( argv[2] );

    if ( stat( original, &originalInfo ) == -1 )
    {
        switch (errno)
        {
        default:
            return printErrno( "unable to get information about \"%s\"", original );
        }
    }

    switch ( originalInfo.st_mode & S_IFMT )
    {
    case S_IFREG:
        break;

    default:
        fprintf( stderr, "%s: original file \"%s\"must be a regular file\n", gInvokedAs, original );
        return -99;
    }

    if ( stat( target, &targetInfo ) == -1 )
    {
        int mode;

        switch ( errno )
        {
        case ENOENT:
            mode = originalInfo.st_mode;
            /* since we're copying a file's perms to directories,
             * mirror each 'read' perm to the 'search' perm */
            mode |= (mode & (S_IRUSR | S_IRGRP | S_IROTH )) >> 2;

            /* dirname modifies its parameter, so dup it first */
            char * path = strdup( target );
            if (createPath( dirname( path ), mode ) == 0)
            {
                if ( link(original, target ) == -1 )
                {
                    return printErrno( "linking \"%s\" to \"%s\" failed", original, target );
                }
            }
            free(path);
            break;

        default:
            return printErrno( "unable to get information about target \"%s\"", target );
        }
    }
    else
    {
#if 0
        fprintf( stderr, "%s: target \"%s\" is an existing %s\n",
                 gInvokedAs, target, typeStrMap[targetInfo.st_mode & S_IFMT] );
#endif

        char * ext = "";

        char * lastPeriod = strrchr( target, '.');
        if ( lastPeriod != NULL && strlen( lastPeriod ) > 5 )
        { /* what we found is too long to be a 'proper' extension */
            lastPeriod = NULL;
        }
        if ( lastPeriod != NULL)
        {
            ext = strdup( lastPeriod ); /* remember the extension (including period) */
            *lastPeriod = '\0';         /* lop off the extension */
        }

        int i = 2;
        do {
            char newTarget[PATH_MAX];
            snprintf( newTarget, sizeof( newTarget ), "%s (%d)%s", target, i, ext );
            if ( stat( newTarget, &targetInfo ) == -1 )
            {
                switch (errno)
                {
                case ENOENT: /* success - we found a path that doesn't already exist */
                    if ( link( original, newTarget ) == -1 )
                    {
                        return printErrno( "linking \"%s\" to \"%s\" failed", original, newTarget );
                    }
                    return 0;

                default:
                    return printErrno( "unable to get information about new target \"%s\"", newTarget );
                }
            }
            /* else we got info, something already exits, try again */
        } while ( ++i < 100 );

        fprintf( stderr, "%s: can't find a target name that doesn't already exist.", gInvokedAs );
        return 99;
    }

    return 0;
}
