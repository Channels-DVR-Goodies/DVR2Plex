//
// Created by Paul on 4/4/2019.
//
// ToDo: #define __STDC_ISO_10646__

#include "chandvr2plex.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#define _XOPEN_SOURCE 1
#define __USE_XOPEN 1
#include <time.h>

/*
 * patterns:
 * SnnEnn
 * nXnn
 * nnXnn
 * nnnn-nn-nn
 * (nnnn)
 */

typedef enum
{
    kNull = 0,
    kNumber
} tCharClass;

typedef unsigned char byte;
typedef unsigned long tHash;

tCharClass hashChar[256] = {
    kNull,              0x01,               0x02,               0x03,
    0x04,               0x05,               0x06,               0x07,
    0x08,               0x09,               0x0a,               0x0b,
    0x0c,               0x0d,               0x0e,               0x0f,
    0x10,               0x11,               0x12,               0x13,
    0x14,               0x15,               0x16,               0x17,
    0x18,               0x19,               0x1a,               0x1b,
    0x1c,               0x1d,               0x1e,               0x1f,
    ' ',                kNull, /* ! */      '"',                '#',
    '$',                '%',                '&',                kNull   /* ' */,
    '(',                ')',                '*',                '+',
    ',',                '-',                '.',                '/',
    kNumber /* 0 */,    kNumber /* 1 */,    kNumber /* 2 */,    kNumber /* 3 */,
    kNumber /* 4 */,    kNumber /* 5 */,    kNumber /* 6 */,    kNumber /* 7 */,
    kNumber /* 8 */,    kNumber /* 9 */,    ':',                ';',
    '<',                '=',                '>',                kNull, /* ? */
    '@',                'a',                'b',                'c',    /* map uppercase to lowercase */
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                '[',
    '\\',               ']',                '^',                '_',
    '`',                'a',                'b',                'c',
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                '{',
    '|',                '}',                '~',                0x7f,
    0x80,               0x81,               0x82,               0x83,
    0x84,               0x85,               0x86,               0x87,
    0x88,               0x89,               0x8a,               0x8b,
    0x8c,               0x8d,               0x8e,               0x8f,
    0x90,               0x91,               0x92,               0x93,
    0x94,               0x95,               0x96,               0x97,
    0x98,               0x99,               0x9a,               0x9b,
    0x9c,               0x9d,               0x9e,               0x9f,
    0xa0,               0xa1,               0xa2,               0xa3,
    0xa4,               0xa5,               0xa6,               0xa7,
    0xa8,               0xa9,               0xaa,               0xab,
    0xac,               0xad,               0xae,               0xaf,
    0xb0,               0xb1,               0xb2,               0xb3,
    0xb4,               0xb5,               0xb6,               0xb7,
    0xb8,               0xb9,               0xba,               0xbb,
    0xbc,               0xbd,               0xbe,               0xbf,
    0xc0,               0xc1,               0xc2,               0xc3,
    0xc4,               0xc5,               0xc6,               0xc7,
    0xc8,               0xc9,               0xca,               0xcb,
    0xcc,               0xcd,               0xce,               0xcf,
    0xd0,               0xd1,               0xd2,               0xd3,
    0xd4,               0xd5,               0xd6,               0xd7,
    0xd8,               0xd9,               0xda,               0xdb,
    0xdc,               0xdd,               0xde,               0xdf,
    0xe0,               0xe1,               0xe2,               0xe3,
    0xe4,               0xe5,               0xe6,               0xe7,
    0xe8,               0xe9,               0xea,               0xeb,
    0xec,               0xed,               0xee,               0xef,
    0xf0,               0xf1,               0xf2,               0xf3,
    0xf4,               0xf5,               0xf6,               0xf7,
    0xf8,               0xf9,               0xfa,               0xfb,
    0xfc,               0xfd,               0xfe,               0xff
};

typedef struct tParam {
    struct tParam * next;
    tHash           hash;
    char *          value;
} tParam;

tParam * head = NULL;

int addParam( tHash hash, char * value )
{
    int result = -1;

    tParam * p = malloc( sizeof(tParam) );

    if (p != NULL)
    {
        p->hash  = hash;
        p->value = value;

        p->next = head;
        head = p;

        result = 0;
    }
    return result;
}

char * findValue( tHash hash )
{
    char * result = NULL;
    tParam * p = head;

    while (p != NULL)
    {
        if ( p->hash == hash )
        {
            result = p->value;
            break;
        }
        p = p->next;
    }
    return result;
}

void freeParams( void )
{
    tParam * p;

    p = head;
    head = NULL;
    while ( p != NULL )
    {
        tParam * next;

        next = p->next;
        free( p );
        p = next;
    }
}

void generateMapping(void)
{
    char *out;
    for (int i = 0; i < 256; ++i)
    {
        switch (i)
        {
        case '\'':
            printf("\tkNull\t/* %c */,", i );
            break;

        default:
            if (isprint(i))
            {
                if ( isdigit( i ) )
                {
                    printf( "\tkNumber /* %c */,", i );
                }
                else
                {
                    printf( "\t\'%c\',\t", tolower( i ) );
                }
            }
            else
            {
                printf( "\t0x%02x,\t", i );
            }
            break;
        }
        printf("%c", (i%4 == 3)? '\n': '\t');
    }
}

/* patterns in the source */
#define kHashYear           0x0000000152354555  // (yyyy)
#define kHashSnnEnn         0x00000003c7c0f9bd  // SnnEnn
#define kHashSyyyyEnn       0x00001bb6ccda0fbd  // SyyyyEnn
#define kHashOneDash        0x000000000000002d  // -
#define kHashDate           0x0001d10a22859cbd  // yyyy-mm-dd
#define kHashDateTime       0x61e28d729df2157d  // yyyy-mm-dd-hhmm

/* keywords in the template */
#define kHashSource         0x0000000416730735
#define kHashSeries         0x00000003d1109b5f
#define kHashEpisode        0x00000099d3300841
#define kHashTitle          0x000000001857f9b5
#define kHashSeason         0x000000043a32a26c
#define kHashExtension      0x00045d732bb4c26c
#define kHashSeasonFolder   0x26b2db9d04411e4c

int parseName( char *name )
{
    int histogram[256];

    memset( histogram, 0, sizeof(histogram));
    for ( char * s = name; *s != '\0'; ++s )
    {
        // don't count all the periods in acronyms like S.W.A.T. and S.H.I.E.L.D.
        if ( *s != '.' || !isprint( s[1] ) || s[2] != '.' )
        {
            histogram[ *s ] += 1;
        }
    }

    unsigned char sep = ' ';
    if (histogram['.'] > histogram[sep])
        sep = '.';
    if (histogram['_'] > histogram[sep])
        sep = '_';
    if (histogram['-'] > histogram[sep])
        sep = '-';


    char seenSeries = 0;

    char *src = name;
    char *dest = strdup( src ); // copy it, because we'll modify it as we go
    char *start = dest;
    char *prevSep = NULL;
    tHash hash = 0;

    unsigned char c;
    do {
        c = *src;
        if ( c != sep && c != '\0' )
        {
            *dest = c;
            if ( hashChar[c] != kNull ) /* we ignore some characters when calculating the hash */
            {
                hash ^= hash * 43 + hashChar[ c ];
            }
        }
        else
        {
            // we reached a separator or the end of the string
            *dest = '\0';

            switch (hash)
            {
                // hashes of the patterns we're looking for.
            case kHashYear:
            case kHashSnnEnn:
            case kHashSyyyyEnn:
            case kHashOneDash:
            case kHashDate:
            case kHashDateTime:
                if ( prevSep != NULL )
                {
                    // first store the run of whatever hasn't hashed to something we recognize
                    *prevSep = '\0';
                    fprintf( stdout, "prev: \'%s\'\n", start);

                    if (seenSeries)
                        addParam( kHashTitle, start );
                    else
                    {
                        addParam( kHashSeries, start );
                        seenSeries = 1;
                    }

                    // now point at the hash match, which is just past prevSep
                    start = &prevSep[1];
                    addParam( hash, start );
                }
                prevSep = NULL;
                break;

            default:
                if ( c == '\0' )
                {
                    fprintf( stdout, "final: \'%s\'\n", start);

                    if (seenSeries)
                        addParam( kHashTitle, start );
                    else
                    {
                        addParam( kHashSeries, start );
                        seenSeries = 1;
                    }
                }
                else
                {
                    *dest = ' ';
                }
                prevSep = dest; // may need this on the next iteration
                break;
            }

            fprintf( stdout, "current: \'" );
            fwrite( start, dest - start, sizeof(char), stdout);
            fprintf( stdout, "\' = %016lx\n", hash );

            unsigned int season  = 0;
            unsigned int episode = 0;
            struct tm firstAired;
            struct tm dateRecorded;
            struct tm year;
            char  temp[50];

            switch (hash)
            {
            case kHashYear: // we found '(nnnn)'
                printf( "(year) = %s\n", start );
                memset( &year, 0, sizeof(year) );

                start = &dest[1];
                break;

            case kHashSnnEnn:   // we found 'SnnEnn'
            case kHashSyyyyEnn: // or 'SyyyyEnn'
                printf("SnnEnn = %s\n", start);
                sscanf( start, "%*c%u%*c%u", &season, &episode );

                snprintf( temp, sizeof(temp), "%02u", season );
                addParam( kHashSeason, strdup( temp ) );
                if ( season == 0 )
                {
                    addParam( kHashSeasonFolder, "Specials" );
                }
                else
                {
                    snprintf( temp, sizeof(temp), "Season %02u", season );
                    addParam( kHashSeasonFolder, strdup( temp ) );
                }

                snprintf( temp, sizeof(temp), "%02u", episode );
                addParam( kHashEpisode, strdup( temp ) );

                start = &dest[1];
                break;

            case kHashOneDash:
                printf("one dash = %s\n", start);
                start = &dest[1];
                break;

            case kHashDate:
                printf("yyyy-mm-dd = %s\n", start);
                strptime( start, "%Y-%m-%d", &firstAired );
                strftime( (char * restrict)temp, sizeof(temp), "%x", &firstAired );
                printf("first aired: %s\n", temp);
                start = &dest[1];
                break;

            case kHashDateTime:
                printf("yyyy-mm-dd-hhmm = %s\n", start);
                strptime( start, "%Y-%m-%d-%H%M", &dateRecorded);
                strftime( (char * restrict)temp, sizeof(temp), "%x %X", &dateRecorded );
                printf("recorded: %s\n", temp);
                start = &dest[1];
                break;

            default:
                break;
            }
            hash = 0;
        }
        ++src;
        ++dest;
    } while ( c != '\0' );

    *dest = '\0';

}

/*
 * carve up the path into directory path, basename and extension
 */
int parsePath( char *path )
{
    int result = 0;

    addParam( kHashSource, path );

    char *lastPeriod = strrchr( path, '.' );
    if (lastPeriod != NULL) {
        addParam( kHashExtension, strdup( lastPeriod ) );
    }
    else
    {
        lastPeriod = path + strlen( path );
    }

    char *lastSlash = strrchr( path, '/' );
    if ( lastSlash != NULL )
    {
        ++lastSlash;
    }
    else
    {
        lastSlash = path; // no directories prefixed
    }

    parseName( strndup( lastSlash, lastPeriod - lastSlash ) );

    return result;
}

tHash hashString( unsigned char *s, unsigned char separator )
{
    tHash result = 0;
    while (*s != '\0' && *s != separator)
    {
        if ( hashChar[*s] != kNull ) /* we ignore some characters when calculating the hash */
        {
            result ^= result * 43 + hashChar[ *s ];
        }
        ++s;
    }
    return result;
}


char *buildString( const char *template )
{
    const char *t = template;
    char *result = NULL;
    char *s;    // pointer into the returned string
    char c;
    const char * k; // beginning of keyword

    result = malloc(32768);
    s = result;

    if (s != NULL)
    {
        c = *t++;
        while ( c != '\0' )
        {
            if ( c != '%' )
            {
                *s++ = c;
            }
            else
            {
                k = t; // remember where the keyword starts
                c = *t++;
                 if ( c == '%' )
                {
                    *s++ = '%';
                }
                else
                {
                    // scan the keyword and generate its hash
                    unsigned long hash = 0;

                    while ( c != '\0' && c != '%' && c != '?')
                    {
                        if ( hashChar[ c ] != kNull ) /* we ignore some characters when calculating the hash */
                        {
                            hash ^= hash * 43 + hashChar[ c ];
                        }
                        c = *t++;
                    }

                    char * value = findValue( hash );

                    if ( value == NULL )
                    {
                        // let's see if it's an environment variable
                        char * envkey = strndup( k, t - k - 1 );
                        value = getenv( envkey );
                        printf("env=\"%s\", value=\"%s\"\n", envkey, value );
                        free( envkey );
                    }

                    if ( c != '?' )
                    {
                        // ether the trailing percent, or null terminator
                        s = stpcpy( s, value );
                    }
                    else
                    {  // trinary operator :)

                        c = *t++;

                        // if undefined, skip over 'true' pattern, find the ':' (or trailing '%')
                        if ( value == NULL )
                        {
                            while (c != '\0' && c != ':' && c != '%')
                            {
                                c = *t++;
                            }

                            if ( c == ':' ) // did we find the 'false' clause?
                            {
                                c = *t++;  // yep, so swallow the colon or the next loop will immediately terminate
                            }
                        }

                        while ( c != '\0' && c != ':' && c != '%' )
                        {
                            if ( c != '@' )
                            {
                                *s++ = c;
                            }
                            else if ( value != NULL )
                            {
                                s = stpcpy( s, value );
                            }

                            c = *t++;
                        }

                        if ( c == ':' ) // skip over the 'false' clause
                        {
                            while ( c != '\0' && c != '%' )
                            {
                                c = *t++;
                            }
                        }
                    }
                }
                if ( c == '\0' )
                    break;
            }
            c = *t++;
        }

        *s = '\0';

        printf( "template = %s\n", template );
        printf( "string = %s\n", result );
    }
    return result;
}

int main( int argc, char * argv[] )
{
    // generateMapping();
    for (int i = 1; i < argc; ++i )
    {
        parsePath( argv[i] );

        free( buildString( "\"%source%\" \"%HOME?@/%%series%/%seasonfolder?@/%%series% %season?@x%%episode?@:-% %title%%extension%\"" ) );
    }
    exit( 0 );
}
