/**
   Copyright &copy; Paul Chambers, 2019.

   @ToDo Switch to UTF-8 string handling, rather than relying on ASCII backwards-compatibility
*/

#define _XOPEN_SOURCE 700
#include <features.h>

#include "chandvr2plex.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <libgen.h> // for basename()
#include <pwd.h>
#define __USE_MISC  // dirent.d_type is linux-specific, apparently
#include <dirent.h>
#define __USE_GNU
#include <unistd.h>
#include <sys/stat.h>

#include <dlfcn.h>
#include <MediaInfoDLL/MediaInfoDLL.h>

#include "dictionary.h"

typedef enum
{
    kIgnore   = ' ', // do not start at zero - avoid possible confusion with string termination
    kNumber   = '0',
    kLBracket = '(',
    kRBracket = ')'
} tCharClass;

int gDebugLevel = 3;

/*
 * this hash table is used to generate hashes used to match patterns.
 * it maps all digits to the same value, maps uppercase letters to
 * lowercase, and ignores several characters completely.
 */
tCharClass hashPattern[256] = {
    '\0',               0x01,               0x02,               0x03,
    0x04,               0x05,               0x06,               0x07,
    0x08,               0x09, /* TAB */     0x0a, /* LF */      0x0b,
    0x0c,               0x0d, /* CR */      0x0e,               0x0f,
    0x10,               0x11,               0x12,               0x13,
    0x14,               0x15,               0x16,               0x17,
    0x18,               0x19,               0x1a,               0x1b,
    0x1c,               0x1d,               0x1e,               0x1f,
    kIgnore, /* ' ' */  kIgnore, /* ! */    '"',                '#',
    '$',                '%',                '&',                kIgnore, /* ' */
    '(',                ')',                '*',                '+',
    ',',                kIgnore, /* - */    kIgnore, /* . */    '/',
    kNumber, /* 0 */    kNumber, /* 1 */    kNumber, /* 2 */    kNumber, /* 3 */
    kNumber, /* 4 */    kNumber, /* 5 */    kNumber, /* 6 */    kNumber, /* 7 */
    kNumber, /* 8 */    kNumber, /* 9 */    ':',                ';',
    '<',                '=',                '>',                kIgnore, /* ? */
    '@',                'a',                'b',                'c',    /* map uppercase to lowercase */
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                '[',
    '\\',               ']',                '^',                kIgnore, /* _ */
    '`',                'a',                'b',                'c',
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                '{',
    '|',                '}',                '~',                0x7f,
#ifndef USE_EXTENDED_ASCII
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
#else
    0x80,               0x81,               0x82,               0x83,
    0x84,               0x85,               0x86,               0x87,
    0x88,               0x89,               'S',                '\'',
    0x8c,               0x8d,               'Z',                0x8f,
    0x80,               '\'',               '\'',               '"',
    '"',                '*',                '-',                '-',
    '~',                0x99,               'S',                '\'',
    0x9c,               0x9d,               'z',                'Y',
    ' ',                '!',                0xa2,               0xa3,
    0xa4,               0xa5,               '|',                0xa7,
    0xa8,               0xa9,               0xaa,               '"',
    0xac,               0xad,               0xae,               0xaf,
    0xb0,               0xb1,               0xb2,               0xb3,
    0xb4,               0xb5,               0xb6,               0xb7,
    0xb8,               0xb9,               0xba,               '"',
    0xbc,               0xbd,               0xbe,               0xbf,
    'A',                'A',                'A',                'A',
    'A',                'A',                0xc6,               'C',
    'E',                'E',                'E',                'E',
    'I',                'I',                'I',                'I',
    'D',                'N',                'O',                'O',
    'O',                'O',                'O',                'x',
    'O',                'U',                'U',                'U',
    'U',                'Y',                0xde,               0xdf,
    'a',                'a',                'a',                'a',
    'a',                'a',                0xe6,               'c',
    'e',                'e',                'e',                'e',
    'i',                'i',                'i',                'i',
    'o',                'n',                'o',                'o',
    'o',                'o',                'o',                0xf7,
    'o',                'u',                'u',                'u',
    'u',                'y',                0xfe,               'y'
#endif
};

/*
 * This hash table is used for series, and parameter names.
 *
 * Period is ignored, because the last one is often omitted of series like 'S.W.A.T.'
 * and 'Marvel's Agents of S.H.I.E.L.D.'. By ignoring it, 'S.W.A.T.', 'S.W.A.T' and
 * 'SWAT' will all result in the same hash value.
 *
 * Since periods can also be used as a separator, we have to treat ' ' and '_'
 * the same way, or the hash for a space-separated name won't match the hash of
 * a period-separated one.
 *
 * In other words, ' ', '_' and '.' do not contribute to the series hash. Similarly
 * '\'' is also often omitted ("Marvel's" becomes "Marvels"), so is similarly
 * ignored when generating a hash, along with '?' (e.g. 'Whose Line Is It Anyway?'
 * and '!' ('Emergency!')
 *
 * "Marvel's Agents of S.H.I.E.L.D. (2017)" as a destination folder is perhaps one
 * of the most difficult matching examples I've seen in the wild. There are so many
 * variations of how to mangle that.
 *
 * ':' is usually converted to '-' or omitted entirely, so ignore those, too.
 *
 * Left and right brackets are also mapped to be equivalent, e.g. [2017] has the
 * same hash as (2017).
 */
tCharClass hashKey[256] = {
    '\0',               0x01,               0x02,               0x03,
    0x04,               0x05,               0x06,               0x07,
    0x08,               kIgnore, /* TAB */  kIgnore, /* LF */   0x0b,
    0x0c,               kIgnore, /* CR */   0x0e,               0x0f,
    0x10,               0x11,               0x12,               0x13,
    0x14,               0x15,               0x16,               0x17,
    0x18,               0x19,               0x1a,               0x1b,
    0x1c,               0x1d,               0x1e,               0x1f,
    kIgnore, /* ' ' */  kIgnore, /* ! */    '"',                '#',
    '$',                '%',                '&',                kIgnore,   /* ' */
    kLBracket, /* ( */  kRBracket, /* ) */  '*',                '+',
    ',',                kIgnore, /* - */    kIgnore, /* . */    '/',
    '0',                '1',                '2',                '3',
    '4',                '5',                '6',                '7',
    '8',                '9',                kIgnore, /* : */    ';',
    '<',                '=',                '>',                kIgnore, /* ? */
    '@',                'a',                'b',                'c',   /* map uppercase to lowercase */
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                kLBracket, /* [ */
    '\\',               kRBracket, /* ] */  '^',                kIgnore,   /* _ */
    '`',                'a',                'b',                'c',
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                kLBracket, /* { */
    '|',                kRBracket, /* } */  '~',                0x7f,
#ifndef USE_EXTENDED_ASCII
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
#else
    0x80,               0x81,               0x82,               0x83,
    0x84,               0x85,               0x86,               0x87,
    0x88,               0x89,               'S',                '\'',
    0x8c,               0x8d,               'Z',                0x8f,
    0x80,               '\'',               '\'',               '"',
    '"',                '*',                '-',                '-',
    '~',                0x99,               'S',                '\'',
    0x9c,               0x9d,               'z',                'Y',
    ' ',                '!',                0xa2,               0xa3,
    0xa4,               0xa5,               '|',                0xa7,
    0xa8,               0xa9,               0xaa,               '"',
    0xac,               0xad,               0xae,               0xaf,
    0xb0,               0xb1,               0xb2,               0xb3,
    0xb4,               0xb5,               0xb6,               0xb7,
    0xb8,               0xb9,               0xba,               '"',
    0xbc,               0xbd,               0xbe,               0xbf,
    'A',                'A',                'A',                'A',
    'A',                'A',                0xc6,               'C',
    'E',                'E',                'E',                'E',
    'I',                'I',                'I',                'I',
    'D',                'N',                'O',                'O',
    'O',                'O',                'O',                'x',
    'O',                'U',                'U',                'U',
    'U',                'Y',                0xde,               0xdf,
    'a',                'a',                'a',                'a',
    'a',                'a',                0xe6,               'c',
    'e',                'e',                'e',                'e',
    'i',                'i',                'i',                'i',
    'o',                'n',                'o',                'o',
    'o',                'o',                'o',                0xf7,
    'o',                'u',                'u',                'u',
    'u',                'y',                0xfe,               'y'
#endif
};


/**
 * trim any trailing whitespace from the end of the string
 *
 * @param line
 */
void trimTrailingWhitespace(char * line)
{
    char * t = line;
    char * nwsp = line;

    if ( t != NULL )
    {
        while (*t != '\0')
        {
            if (!isspace(*t++))
            {
                // note: t has already been incremented
                nwsp = t;
            }
        }
        *nwsp = '\0';
    }
}


/**
 * this hashes the 'series' using the 'key' hash table,
 * since comparing series names needs different logic
 * than scanning for patterns.
 * Separators (spaces, periods, underscores) are ignored
 * completely. As are \', !, amd ?, since those are
 * frequently omitted. Upper case letters are mapped to
 * lower case since those are also very inconsistent
 * (no UTF-8 handling, though). and '&' is expanded to
 * 'and' in the hash, so both forms will match.
 */
void addSeries( tDictionary * dictionary, string series )
{
    tHash result = 0;
    unsigned char * s = (unsigned char *)series;
    unsigned char  c;

    do {
        c = hashKey[ *s++ ];
        switch ( c )
        {
        case '\0':
            break;

            // we hash the '&' character as if 'and' was used. so both forms generate the same hash
            // e.g. the hash of 'Will & Grace' will match the hash of 'Will and Grace'
        case '&':
            result ^= result * 43 + 'a';
            result ^= result * 43 + 'n';
            result ^= result * 43 + 'd';
            break;

        case kLBracket:
            // we found something bracketed, e.g. (uk) or (2019)
            // so we also add the intermediate hash to the dictionary,
            // before we hash the bracketed content. Then the same series with
            // the year omitted, for example,  will still match something.
            // Though we can't do much about a file that only has 'MacGyver'
            // when it's actually part of the 'MacGyver (2016)' series.
            //
            // Note: if there are multiple left brackets, there will be multiple hashes added

            addParam( dictionary, result, series );
            result ^= result * 43 + c;
            break;

        default:
            if (c != kIgnore)
            {
                result ^= result * 43 + c;
            }
            break;
        }
    } while ( c != '\0' );

    // also add the hash of the full string, including any trailing bracketed stuff
    addParam( dictionary, result, series );
}

static int scanDirFilter( const struct dirent * entry)
{
    int result = 0;

    result = ( entry->d_name[0] != '.' && entry->d_type == DT_DIR );

    // debugf( 3, "%s, 0x%x, %d\n", entry->d_name, entry->d_type, result );
    return result;
}

int buildSeriesDictionary( tDictionary * dictionary, string path )
{
    struct dirent **namelist;
    int n;

    n = scandir( path, &namelist, scanDirFilter, alphasort);
    if ( n < 0 ) {
        perror("scandir");
        return n;
    }

    for ( int i = 0; i < n; ++i )
    {
        addSeries( dictionary, namelist[ i ]->d_name );
        free( namelist[ i ] );
    }
    free(namelist);

    /* printDictionary( dictionary ); */

    return 0;
}

#if 0
/* this may execute more than once, longest match will be returned */
string lookupSeries( tDictionary * dictionary, string series )
{
    string result = NULL;
    string destSeries;
    tHash hash = 0;
    unsigned char * s;
    unsigned char   c;

    s = (unsigned char *)series;
    do {
        c = hashKey[ *s++ ];
        switch ( c )
        {
        case kIgnore:
        case '\0':
            debugf(3, "checking: %016lx\n", hash);

            destSeries = findValue( dictionary, hash);
            if (destSeries != NULL)
            {
                debugf(3, "match: %s\n", destSeries);
                result = destSeries;
            }
            break;

                /* we hash the '&' character as if 'and' was used. so both forms generate the same
                   hash e.g. the hash of 'Will & Grace' will match the hash of 'Will and Grace' */
        case '&':
            hash ^= hash * 43 + 'a';
            hash ^= hash * 43 + 'n';
            hash ^= hash * 43 + 'd';
            break;

        case kLBracket:
            /* we found something bracketed, e.g. (uk) or (2019) so we
               also check the intermediate hash against the dictionary */
            debugf( 3, "checking: %016lx\n", hash);

            destSeries = findValue( dictionary, hash );
            if (destSeries != NULL )
            {
                result = destSeries;
            }
            hash ^= hash * 43 + c;
            break;

        default:
            if (c != kIgnore)
            {
                hash ^= hash * 43 + c;
            }
            break;
        }
    } while ( c != '\0' );

    return result;
}
#endif


void addSeasonEpisode( tDictionary * dictionary, unsigned int season, unsigned int episode )
{
    char  temp[50];

    snprintf( temp, sizeof(temp), "%02u", season );
    addParam( dictionary, kKeySeason, temp );
    if ( season == 0 )
    {
        addParam( dictionary, kKeySeasonFolder, "Specials" );
    }
    else
    {
        snprintf( temp, sizeof(temp), "Season %02u", season );
        addParam( dictionary, kKeySeasonFolder, temp );
    }

    snprintf( temp, sizeof(temp), "%02u", episode );
    addParam( dictionary, kKeyEpisode, temp );
}

int storeParam( tDictionary *dictionary, tHash hash, string value )
{
    // struct tm firstAired;
    // struct tm dateRecorded;
    // struct tm year;
    unsigned int season  = 0;
    unsigned int episode = 0;

    switch (hash)
    {
    case kPattern_SnnEnn:   // we found 'SnnEnn' or
    case kPattern_SyyyyEnn: // SyyyyEnnn
    case kPattern_SnnEn:    // SnnEn
    case kPattern_SnEnn:    // SnEnn
    case kPattern_SnEn:     // SnEn
        debugf( 3,"SnnEnn: %s\n", value);
        sscanf( value, "%*1c%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kPattern_Ennn:
        debugf( 3,"Ennn: %s\n", value);
        sscanf( value, "%*1c%u", &episode ); // ignore characters since we don't know their case
        season = episode / 100;
        episode %= 100;
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kPattern_Ennnn:
        debugf( 3,"Ennnn: %s\n", value);
        sscanf( value, "%*1c%u", &episode ); // ignore characters since we don't know their case
        unsigned int divisor = 100;
        /* see if there's a season number to extract */
        if ( ((episode / divisor) % 10) == 0 )
        {
            /* least significant digit of season is zero, so we can increase the divisor by 10 */
            divisor *= 10;
        }
        season = episode / divisor;
        episode %= divisor;
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kPattern_nXnn:
    case kPattern_nnXnn:
        debugf( 3,"nnXnn: %s\n", value);
        sscanf( value, "%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kPattern_Date:
        debugf( 3,"yyyy-mm-dd: %s\n", value);
        addParam( dictionary, kKeyFirstAired, value );
        break;

    case kPattern_DateTime:
        debugf( 3,"yyyy-mm-dd-hhmm: %s\n", value);
        addParam( dictionary, kKeyDateRecorded, value );
        break;

    case kPattern_TwoDigits:
    case kPattern_FourDigits:
    case kPattern_SixDigits:
        break;

    default:
        break;
    }
    return 0;
}

tHash checkHash( tHash hash)
{
    switch (hash)
    {
    case kPattern_SnnEnn:   // SnnEnn
    case kPattern_SyyyyEnn: // SnnnnEnn
    case kPattern_SnnEn:    // SnnEn
    case kPattern_SnEnn:    // SnEnn
    case kPattern_SnEn:     // SnEn
    case kPattern_Ennn:     // Ennn
    case kPattern_Ennnn:    // Ennnn
    case kPattern_nXnn:     // nXnn
    case kPattern_nnXnn:    // nnXnn
//    case kPattern_Date:     // yyyy-mm-dd
//    case kPattern_DateTime: // yyyy-mm-dd-hhmm
    case kPattern_TwoDigits:
    case kPattern_FourDigits:
    case kPattern_SixDigits:
        break;

    default:
        hash = kPattern_noMatch;
        break;
    }
    return hash;
}



int parseName( tDictionary *fileDict, tDictionary *seriesDict, string name )
{
    string temp = strdup(name); // copy it, because we'll terminate strings in place as we go

    if (temp != NULL) {
        struct {
            unsigned char * start;
            unsigned char * end;
            tHash hash;
            unsigned char seperator;
        } tokens[32];

        unsigned int i, j, k;
        unsigned char c;

        unsigned char *start = (unsigned char *) temp;
        unsigned char *ptr = start;
        tHash hash = 0;

        i = 1;

        start = (unsigned char *) temp;
        ptr = start;
        hash = 0;
        tokens[0].hash = kPattern_noMatch;
        tokens[0].start = ptr;

        do {
            c = hashPattern[*ptr];
            switch (c) {
                case kIgnore:
                case '\0':
                    debugf(3, "hash: 0x%016lx \'%s\'\n", hash, start );
                    tokens[i].hash      = checkHash( hash );
                    tokens[i].start     = start;
                    tokens[i].end       = ptr;
                    tokens[i].seperator = *ptr;
                    i++;
                    hash = 0;
                    start = ptr + 1;
                    break;

                case '&':
                    hash ^= hash * 43 + 'a';
                    hash ^= hash * 43 + 'n';
                    hash ^= hash * 43 + 'd';
                    break;

                default:
                    hash ^= hash * 43 + c;
                    break;
            };
            ptr++;
        } while (c != '\0' && i < 32);

        unsigned int runCount = 0;

        j = k = 1;
        while ( k < i )
        {
            if ( j != k )
            {
                tokens[j].start = tokens[k].start;
                tokens[j].hash  = tokens[k].hash;
            }
            /* merge adjacent tokens with unknown hashes to a single run */
            if ( tokens[k].hash == kPattern_noMatch )
            {
                runCount++;

                while ( tokens[k].hash == kPattern_noMatch && k < i )
                {
                    k++;
                }
                k--;
            }
            if ( j != k )
            {
                tokens[j].end       = tokens[k].end;
                tokens[j].seperator = tokens[k].seperator;
            }
            *tokens[j].end = '\0';

            string series = NULL;
            string start = (string) tokens[j].start;
            unsigned char * end;

            ptr  = (unsigned char *) start;
            hash = 0;

            if ( tokens[j].hash == kPattern_noMatch )
            {
                switch ( runCount )
                {
                case 1:
                    do
                    {
                        c = hashKey[*ptr];
                        switch ( c )
                        {
                        case kIgnore:
                        case '\0': /* let's see if we have a match */
                            debugf( 3, "checking: 0x%016lx\n", hash );

                            string match = findValue( seriesDict, hash );
                            if ( match != NULL)
                            {
                                series = match;
                                debugf( 3, "matched %s\n", series );
                                end = ptr;
                            }
                            break;

                        case '&':
                            hash ^= hash * 43 + 'a';
                            hash ^= hash * 43 + 'n';
                            hash ^= hash * 43 + 'd';
                            break;

                        default:
                            hash ^= hash * 43 + c;
                            break;
                        };
                        ptr++;
                    } while ( c != '\0' );

                    if ( series != NULL)
                    {
                        addParam( fileDict, kKeyDestSeries, series );

                        if ( *end != '\0' )
                        {
                            /* if the run is longer than the series name,
                             * store the remanent as the episode title */
                            addParam( fileDict, kKeyTitle, (string) end + 1 );
                        }
                        *end = '\0';
                        addParam( fileDict, kKeySeries, start );
                    }
                    else
                    {
                        addParam( fileDict, kKeyDestSeries, start );
                        addParam( fileDict, kKeySeries, start );
                    }
                    break;

                default:
                    addParam( fileDict, kKeyTitle, start );
                    break;
                }
            }
            else
            {
                storeParam( fileDict, tokens[j].hash, (string) tokens[j].start );
            }
            j++;
            k++;
        }
        i = j;


        debugf( 3, "run count: %d\n", runCount );
        for ( j = 1; j < i; j++ )
        {
            if ((j + 1) < i )
            {
                *(tokens[j + 1].start - 1) = '\0';
            }
            debugf( 3, "token: \'%s\', \'%s\' (%c)\n", lookupHash( tokens[j].hash ), tokens[j].start, tokens[j].seperator );
        }

        free((void *) temp );
    }

    return 0;
}

/*
 * carve up the path into directory path, basename and extension
 * then pass basename onto parseName() to be processed
 */
int parsePath( tDictionary * fileDict, tDictionary * seriesDict, string path )
{
    int result = 0;

    addParam( fileDict, kKeySource, path );

    string lastPeriod = strrchr( path, '.' );
    if ( lastPeriod != NULL )
    {
        addParam( fileDict, kKeyExtension, lastPeriod );
    }
    else
    {
        lastPeriod = path + strlen( path );
    }

    string lastSlash = strrchr( path, '/' );
    if ( lastSlash != NULL )
    {
        string p = strndup( path, lastSlash - path );
        addParam( fileDict, kKeyPath, p );
        free( (void *)p );

        ++lastSlash;
    }
    else
    {
        lastSlash = path; // no directories prefixed
    }

    string basename = strndup( lastSlash, lastPeriod - lastSlash );
    addParam( fileDict, kKeyBasename, basename );
    parseName( fileDict, seriesDict, basename );
    free( (void *)basename );

    return result;
}

string buildString( tDictionary *mainDict, tDictionary *fileDict, string template )
{
    string result = NULL;
    string t = template;
    char * s;    // pointer into the returned string

    result = calloc( 1, 32768 );
    s = (char *)result;

    if ( s != NULL )
    {
        unsigned char c = *t++; // unsigned because it is used as an array subscript when calculating the hash
        while ( c != '\0' )
        {
            unsigned long hash;
            string k;

            switch (c)
            {
            case '{':   // start of keyword
                    k = t; // remember where the keyword starts
                    c = *t++;

                    // scan the keyword and generate its hash
                    hash = 0;

                    while ( c != '\0' && c != '}' && c != '?' )
                    {
                        if ( hashKey[ c ] != kIgnore ) /* we ignore some characters when calculating the hash */
                        {
                            hash ^= hash * 43 + hashKey[ c ];
                        }
                        c = *t++;
                    }

#if 0
                    string tmpStr = strndup( k, t - k - 1 );
                    debugf( 3, "key \'%s\' = 0x%016lx\n", tmpStr, hash );
                    free( tmpStr );
#endif
                    if ( hash != kKeyTemplate ) // don't want to expand a {template} keyword in a template
                    {
                        string value = findValue( fileDict, hash );

                        if ( value == NULL ) // not in the file dictionary, try the main dictionary
                        {
                            value = findValue( mainDict, hash );
                        }

                        if ( value == NULL ) // not in the main dictionary either, check for an environment variable
                        {
                            string envkey = strndup( k, t - k - 1 );
                            value = getenv( envkey );
                            // debugf( 3, "env=\"%s\", value=\"%s\"\n", envkey, value );
                            free( (void *)envkey );
                        }

                        if ( c != '?' )
                        {
                            // end of keyword, and not the beginning of a trinary expression
                            if ( value != NULL )
                            {
                                s = stpcpy( s, value );
                            }
                        }
                        else
                        {  // trinary operator, like {param?true:false} (true or false can be absent)

                            c = *t++;

                            if ( value != NULL )
                            {
                                // copy the 'true' clause
                                while ( c != '}' && c != ':' && c != '\0' )
                                {
                                    if ( c != '@' )
                                    {
                                        *s++ = c;
                                    }
                                    else
                                    {
                                        s = stpcpy( s, value );
                                    }

                                    c = *t++;
                                }

                                if ( c == ':' )
                                {
                                    // skip over the 'false' clause
                                    while ( c != '\0' && c != '}' )
                                    {
                                        c = *t++;
                                    }
                                }
                            }
                            else // if undefined, skip over 'true' pattern, find the ':' (or trailing '}')
                            {
                                // value is undefined, so skip ahead to the false clause (or keyword end)
                                while ( c != ':' && c != '}' && c != '\0' )
                                {
                                    c = *t++;
                                }

                                if ( c == ':' ) // did we find the 'false' clause?
                                {
                                    c = *t++;  // yep, so swallow the colon
                                    // copy the 'false' clause into the string
                                    // no '@' processing, as the parameter is not defined
                                    while ( c != '\0' && c != '}' )
                                    {
                                        *s++ = c;
                                        c = *t++;
                                    }
                                }
                            }
                        }
                    } // if !{template}
                    break;

                case '\\': // next template character is escaped, not interpreted, e.g. \{
                    c = *t++;
                    *s++ = c;
                    break;

                default:
                    *s++ = c;
                    break;
            } // switch

            c = *t++;
        }

        *s = '\0'; // always terminate the string
    }
    return result;
}

int parseConfigFile( tDictionary * dictionary, string path )
{
    int  result  = 0;
    FILE *file;
    char * buffer = malloc( 4096 ); // 4K seems like plenty

    debugf( 3, "config file: \'%s\'\n", path );
    if ( eaccess( path, R_OK ) == 0 )   // only attempt to parse it if there's something there
    {
        file = fopen(path, "r");
        if (file == NULL)
        {
            fprintf( stderr,
                     "### Error: Unable to open config file \'%s\' (%d: %s)\n",
                     path, errno, strerror(errno) );
            result = -5;
        }
        else
        {
            while ( fgets(buffer, 4096, file) != NULL )
            {
                trimTrailingWhitespace(buffer);
                debugf( 3,"line: \"%s\"\n", buffer);

                tHash hash = 0;
                string s = buffer;
                while (isspace(*s)) {
                    s++;
                }

                unsigned char c = (unsigned char) *s++;
                if (c != '\0') {
                    while (c != '\0' && c != '=') {
                        if (c != kIgnore) {
                            hash ^= hash * 43 + hashKey[c];
                        }
                        c = (unsigned char) *s++;
                    }

                    if (c == '=') {
                        // trim whitespace from the beginning of the value
                        while (isspace(*s)) {
                            s++;
                        }

                        string e = s;
                        string p = s;
                        while (*p != '\0') {
                            if ( !isspace(*p++) ) {
                                e = p; // remember the location just past the most recent non-whitespace char we've seen
                            }
                        }
                        // e should now point just past the last non-whitespace character in the string
                        *(char *)e = '\0'; // trim off any trailing whitespace at the end of the string - including the LF

                    }
                    debugf( 3,"hash = 0x%016lx, value = \'%s\'\n", hash, s);
                    addParam(dictionary, hash, s);
                }
            }
            free(buffer);
            fclose(file);
        }
    }
    else
    {
        if (errno != ENOENT)
        {
            fprintf( stderr,
                     "### Error: Unable to access config file \'%s\' (%d: %s)",
                     path, errno, strerror(errno) );
        }
        result = errno;
    }

    return result;
}

/*
 * look for config files to process. First, look in /etc/<argv[0]>.conf then in ~/.config/<argv[0]>.conf,
 * and finally the file passed as a -c parameter, if any, then any parameters on the command line (except -c)
 * Where a parameter occurs more than once in a dictionary, the most recent definition 'wins'
 */

int parseConfig( tDictionary * dictionary, string path, string myName )
{
    int result = 0;
    char temp[PATH_MAX];

    snprintf( temp, sizeof( temp ), "/etc/%s.conf", myName );
    debugf( 3, "/etc path: \"%s\"\n", temp );

    result = parseConfigFile( dictionary, temp );
    if ( result == ENOENT )
    {
        result = 0;
    }

    if ( result == 0 )
    {
        string home = getenv("HOME");
        if ( home == NULL)
        {
            home = getpwuid(getuid())->pw_dir;
        }
        if ( home != NULL )
        {
            snprintf( temp, sizeof( temp ), "%s/.config/%s.conf", home, myName );
            debugf( 3, "~ path: \"%s\"\n", temp );

            result = parseConfigFile( dictionary, temp );
            if ( result == ENOENT )
            {
                result = 0;
            }
        }
    }

    if ( result == 0 && path != NULL )
    {
        snprintf( temp, sizeof( temp ), "%s/%s.conf", path, myName );
        debugf( 3, "-c path: %s\n", temp );

        result = parseConfigFile( dictionary, temp );
    }

    return result;
}

int processFile( tDictionary * mainDict, tDictionary * seriesDict, string path )
{
    int result = 0;

    string template = findValue( mainDict, kKeyTemplate );
    if ( template == NULL )
    {
        fprintf( stderr, "### Error: no template defined.\n" );
        result = -2;
    }
    else
    {
        debugf( 3, "template = \'%s\'\n", template );

        tDictionary *fileDict = createDictionary( "File" );
        if ( fileDict != NULL )
        {
            parsePath( fileDict, seriesDict, path );

            printDictionary( fileDict );

            string output = buildString( mainDict, fileDict, template );
            string exec   = findValue( mainDict, kKeyExecute );
            if ( exec != NULL )
            {
                result = system(output);
            }
            else
            {
                printf( "%s%c", output, '\n' );
                result = 0;
            }
            free( (void *)output );

            destroyDictionary( fileDict );
        }
    }
    return result;
}

int main( int argc, string argv[] )
{
    int  result       = 0;
    int  cnt          = argc;
    string configPath = NULL;

    tDictionary * mainDict = createDictionary( "Main" );

    string myName = basename( strdup( argv[0] ) ); // posix flavor of basename modifies its argument

    int k = 1;
    for ( int i = 1; i < argc && result == 0; i++ )
    {
        debugf( 3, "a: i = %d, k = %d, cnt = %d, \'%s\'\n", i, k, cnt, argv[ i ] );

        // is it the config file option?
        if ( strcmp( argv[ i ], "-c" ) == 0 )
        {
            cnt -= 2;
            ++i;
            configPath = strdup( argv[ i ] );   // make a copy - argv will be modified
        }
        else
        {
            if ( i != k )
            {
                argv[ k ] = argv[ i ];
            }
            ++k;
        }
    }
    argc = cnt;

    result = parseConfig( mainDict, configPath, myName );

    if ( configPath != NULL )
    {
        free( (void *)configPath );
        configPath = NULL;
    }

    k = 1;
    for ( int i = 1; i < argc && result == 0; i++ )
    {
        debugf( 3, "b: i = %d, k = %d, cnt = %d, \'%s\'\n", i, k, cnt, argv[i] );

        // is it an option?
        if (argv[i][0] == '-' )
        {
            char option = argv[i][1];
            if ( argv[i][2] != '\0' )
            {
                fprintf( stderr, "### Error: option \'%s\' not understood.\n", argv[ i ] );
                result = -1;
            }
            else
            {
                --cnt;

                switch ( option )
                {
                // case 'c':   // config file already handled
                //  break;

                case 'd':   // destination
                    addParam( mainDict, kKeyDestination, argv[ i ] );
                    --cnt;
                    ++i;
                    break;

                case 't':   // template
                    addParam( mainDict, kKeyTemplate, argv[ i ] );
                    --cnt;
                    ++i;
                    break;

                case 'x':   // execute
                    addParam( mainDict, kKeyExecute, "yes" );
                    break;

                case '-':   // also read lines from stdin
                    addParam( mainDict, kKeyStdin, "yes" );
                    break;

                case '0':   // entries from stdio are terminated with NULLs
                    addParam( mainDict, kKeyNullTermination, "yes" );
                    break;

                case 'v': //verbose output, i.e. debug logging
                    if ( i < argc - 1 )
                    {
                        ++i;
                        --cnt;

                        gDebugLevel = atoi( argv[i] );
                        fprintf(stderr, "verbosity = %d\n", gDebugLevel );
                    }
                    break;

                default:
                    ++cnt;
                    --i; // point back at the original option
                    fprintf( stderr, "### Error: option \'%s\' not understood.\n", argv[ i ] );
                    result = -1;
                    break;
                }
            }
        }
        else
        {
            if ( i != k )
            {
                argv[k] = argv[i];
            }
            ++k;
        }
    }
    argc = cnt;

    /* printDictionary( mainDict ); */

    tDictionary *seriesDict = createDictionary( "Series" );

    string destination = findValue( mainDict, kKeyDestination );

    if ( destination == NULL )
    {
        fprintf( stderr, "### Error: no destination defined.\n" );
        result = -3;
    }
    else
    {
        // fill the dictionary with hashes of the directory names in the destination
        buildSeriesDictionary( seriesDict, destination );
    }


    for ( int i = 1; i < argc && result == 0; ++i )
    {
        debugf( 3, "%d: \'%s\'\n", i, argv[ i ] );
        processFile( mainDict, seriesDict, argv[i] );
    }

    // should we also read from stdin?
    if ( findValue( mainDict, kKeyStdin ) != NULL )
    {
        char line[PATH_MAX];

        if ( findValue( mainDict, kKeyNullTermination ) != NULL )
        {
            // lines are terminated by \0
            char * p = line;
            cnt = sizeof( line );

            while (!feof(stdin))
            {
                char c = fgetc( stdin );
                *p++ = c;
                cnt--;

                if ( c == '\0' || cnt < 1 )
                {
                    debugf( 3, "null: %s\n", line );
                    processFile( mainDict, seriesDict, line );

                    p = line;
                    cnt = sizeof( line );
                }
            }
        }
        else
        {
            while (!feof(stdin))
            {
                // lines are terminated by \n
                fgets( line, sizeof(line), stdin );

                // trim the inevitable trailing newline(s)/whitespace
                trimTrailingWhitespace( line );
                debugf( 3,"eol: %s\n", line);
                processFile(mainDict, seriesDict, line);
            }
        }
    }

    // all done, clean up.
    destroyDictionary( mainDict );
    destroyDictionary( seriesDict );


    return result;
}
