//
// Created by Paul on 4/4/2019.
//
// ToDo: switch to UTF-8 string handling, rather than relying on ASCII backwards-compatibility

#define _XOPEN_SOURCE 700
#include <features.h>

#include "chandvr2plex.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#define __USE_MISC  // dirent.d_type is linux-specific, apparently
#include <dirent.h>

#define DEBUG
#ifndef DEBUG
  #define debugf( format, ... )
#else
  #define debugf( format, ... ) fprintf( stderr, format, __VA_ARGS__ )
#endif


typedef enum
{
    kIgnore = 1, // do not start at zero - possible confusion with string termination
    kNumber,
    kSeperator,
    kLBracket,
    kRBracket
} tCharClass;

typedef unsigned char byte;
typedef unsigned long tHash;

tCharClass hashCharMap[256] = {
    '\0',               0x01,               0x02,               0x03,
    0x04,               0x05,               0x06,               0x07,
    0x08,               0x09,               0x0a,               0x0b,
    0x0c,               0x0d,               0x0e,               0x0f,
    0x10,               0x11,               0x12,               0x13,
    0x14,               0x15,               0x16,               0x17,
    0x18,               0x19,               0x1a,               0x1b,
    0x1c,               0x1d,               0x1e,               0x1f,
    ' ',                kIgnore, /* ! */    '"',                '#',
    '$',                '%',                '&',                kIgnore   /* ' */,
    '(',                ')',                '*',                '+',
    ',',                '-',                '.',                '/',
    kNumber /* 0 */,    kNumber /* 1 */,    kNumber /* 2 */,    kNumber /* 3 */,
    kNumber /* 4 */,    kNumber /* 5 */,    kNumber /* 6 */,    kNumber /* 7 */,
    kNumber /* 8 */,    kNumber /* 9 */,    ':',                ';',
    '<',                '=',                '>',                kIgnore, /* ? */
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

/*
 * period is ignored, because it's often omitted at the end of series like 'S.W.A.T.'
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
 * Left and right brackets are also mapped to be equivalent, e.g. [2017] is the
 * hashed the same as (2017).
 */
tCharClass hashSeriesMap[256] = {
    '\0',               0x01,               0x02,               0x03,
    0x04,               0x05,               0x06,               0x07,
    0x08,               0x09,               0x0a,               0x0b,
    0x0c,               0x0d,               0x0e,               0x0f,
    0x10,               0x11,               0x12,               0x13,
    0x14,               0x15,               0x16,               0x17,
    0x18,               0x19,               0x1a,               0x1b,
    0x1c,               0x1d,               0x1e,               0x1f,
    kIgnore, /* ' ' */  kIgnore, /* ! */    '"',                '#',
    '$',                '%',                '&',                kIgnore   /* ' */,
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
    'x',                'y',                'z',                kLBracket,  /* [ */
    '\\',               kRBracket, /* ] */  '^',                kIgnore,    /* _ */
    '`',                'a',                'b',                'c',
    'd',                'e',                'f',                'g',
    'h',                'i',                'j',                'k',
    'l',                'm',                'n',                'o',
    'p',                'q',                'r',                's',
    't',                'u',                'v',                'w',
    'x',                'y',                'z',                kLBracket, /* { */
    '|',                kRBracket, /* } */  '~',                0x7f,
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


typedef struct {
    tParam * head;
} tDictionary;



tDictionary * createDictionary( void )
{
    return (tDictionary *)calloc( 1, sizeof(tDictionary) );
}

void destroyDictionary( tDictionary * dictionary )
{
    tParam * p;

    p = dictionary->head;
    free( dictionary );

    while ( p != NULL )
    {
        tParam * next;

        // debugf( "{%s}\n", p->value );
        next = p->next;
        free( p->value );
        free( p );
        p = next;
    }
}

void printDictionary( tDictionary * dictionary )
{
    if ( dictionary != NULL )
    {
        tParam * p = dictionary->head;
        while ( p != NULL )
        {
            debugf( "0x%016lx, \"%s\"\n", p->hash, p->value );
            p = p->next;
        }
    }
}

int addParam( tDictionary * dictionary, tHash hash, char * value )
{
    int result = -1;

    tParam * p = malloc( sizeof(tParam) );

    if (p != NULL)
    {
        p->hash  = hash;
        p->value = strdup( value );

        p->next = dictionary->head;
        dictionary->head = p;

        result = 0;
    }
    return result;
}

char * findValue( tDictionary * dictionary, tHash hash )
{
    char * result = NULL;
    tParam * p = dictionary->head;

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


void generateMapping( void )
{
    for ( unsigned int i = 0; i < 256; ++i )
    {
        switch (i)
        {
        case '\'':
            printf("\tkIgnore\t/* %c */,", i );
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
/*
 * SnnEnn
 * SnnNnEnn
 * nXnn
 * nnXnn
 * -
 * nnnn-nn-nn
 * nnnn-nn-nn-nnnn
 */
#define kHashSnnEnn         0x00000003c7a2664a  // SnnEnn
#define kHashSyyyyEnn       0x00001bb91564fb4a  // SyyyyEnn
#define kHashSnnEn          0x0000000016b931ac  // SnnEn
#define kHashSnEnn          0x0000000016c43aaa  // SnEnn
#define kHashSnEn           0x000000000084789c  // SnEn
#define kHashnXnn           0x000000000005efba  // nXnn
#define kHashnnXnn          0x000000000070e7ba  // nnXnn
#define kHashOneDash        0x000000000000002d  // -
#define kHashDate           0x0001d10a22859cbd  // yyyy-mm-dd
#define kHashDateTime       0x61e28d729df2157d  // yyyy-mm-dd-hhmm

/* keywords in the template */
#define kHashDestination    0x1f8c5d6e23059b0c
#define kHashTemplate       0x00001c70c5df6271
#define kHashSource         0x0000000416730735
#define kHashPath           0x00000000008bc56c
#define kHashBasename       0x00001770bd72d401
#define kHashSeries         0x00000003d1109b5f
#define kHashEpisode        0x00000099d3300841
#define kHashTitle          0x000000001857f9b5
#define kHashSeason         0x000000043a32a26c
#define kHashExtension      0x00045d732bb4c26c
#define kHashSeasonFolder   0x26b2db9d04411e4c
#define kHashDestSeries     0x00b9cdb100649b5f
#define kHashFirstAired     0x00b3fed4e2899d3e
#define kHashDateRecorded   0x46b649d0032996fe

#if 0
/*
 * this gives a list of hashes where a year is part of the series name.
 * the heuristics can't get this right 100% of the time
 */

struct {
    tHash   hash;
} seriesWhitelist[] =
    {
        { 0x1 },    // 1066: A Year to Conquer England
        { 0x1 },    // 1066: The Battle for Middle Earth
        { 0x1 },    // 1776
        { 0x1 },    // 1906
        { 0x1 },    // 1922
        { 0x1 },    // 1922
        { 0x1 },    // 1942: A Love Story
        { 0x1 },    // 1945
        { 0x1 },    // 1968
        { 0x1 },    // 1969
        { 0x1 },    // 1984
        { 0x1 },    // 1990: The Bronx Warriors
        { 0x1 },    // 1999
        { 0x1 },    // 2001
        { 0x1 },    // 2001: a space odyssey
        { 0x1 },    // 2001: A Space Travesty
        { 0x1 },    // 2009: Lost Memories
        { 0x1 },    // 2010
        { 0x1 },    // 2010: Moby Dick
        { 0x1 },    // 2010: the Year we make contact
        { 0x1 },    // 2012
        { 0x1 },    // 2012: Ice Age
        { 0x1 },    // 2012: Supernova
        { 0x1 },    // 2019: After the Fall of New York
        { 0x1 },    // 2036 Origin Unknown
        { 0x1 },    // 2054
        { 0x1 },    // 2081
        { 0x1 },    // Airport 1975
        { 0x1 },    // Amityville 1992: It's About Time
        { 0x1 },    // Back to 1942
        { 0x1 },    // Blade Runner 2049
        { 0x1 },    // Camille Claudel 1915
        { 0x1 },    // Class of 1984
        { 0x1 },    // Class of 1999
        { 0x1 },    // Death Race 2000
        { 0x1 },    // Death Race 2050
        { 0x1 },    // Le Mans 1955
        { 0x1 },    // Love Story 2050
        { 0x1 },    // Nico, 1988
        { 0x1 },    // Prime Suspect 1973
        { 0x1 },    // Spring 1941
        { 0x1 },    // Summer 1993
        { 0x1 },    // Swing Parade of 1946
        { 0x1 },    // Systrar 1968
        { 0x1 },    // The Big Broadcast of 1938
        { 0x1 },    // Veronica 2030
        { 0 }       // <end of list>
    };

int seriesIsWhitelisted( tHash hash )
{
    tHash * h = seriesWhitelist[0].hash;

    for ( unsigned int i; seriesWhitelist[i] != 0; ++i )
    {
        if ( seriesWhitelist[i].hash == hash )
        {
            return 1;
        }
    }
    return 0;
}
#endif



/*
 * this uses the 'series' hash table, since comparing series
 * names needs different logic than scanning for patterns.
 * Separators (spaces, periods, underscores) are ignored
 * completely. As are \', !, amd ?, since those are
 * frequently omitted. Upper case letters are mapped to
 * lower case since those are also very inconsistent
 * (no UTF-8 handling, though). and '&' is expanded to
 * 'and' in the hash, so both forms will match.
 */
void addSeries( tDictionary * dictionary, char * series )
{
    tHash result = 0;
    unsigned char *s = (unsigned char *) series;
    unsigned char  c = hashSeriesMap[ *s++ ];

    while ( c != '\0' )
    {
        switch ( c )
        {
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
        c = hashSeriesMap[ *s++ ];
    }

    // also add the hash of the full string, including any trailing bracketed stuff
    addParam( dictionary, result, series );
}

static int scanDirFilter( const struct dirent * entry)
{
    int result = 0;

    result = ( entry->d_name[0] != '.' && entry->d_type == DT_DIR );

    debugf( "%s, 0x%x, %d\n", entry->d_name, entry->d_type, result );
    return result;
}

int buildSeriesDictionary( tDictionary * dictionary, char *path )
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

    fprintf(stderr, "series dictionary\n");
    printDictionary( dictionary );

    return 0;
}

char * lookupSeries( tDictionary * dictionary, char * series )
{
    char * result = NULL;
    tHash hash = 0;
    unsigned char *s = (unsigned char *) series;
    unsigned char  c = hashSeriesMap[ *s++ ];


    while ( c != '\0' )
    {
        switch ( c )
        {
            // we hash the '&' character as if 'and' was used. so both forms generate the same hash
            // e.g. the hash of 'Will & Grace' will match the hash of 'Will and Grace'
        case '&':
            hash ^= hash * 43 + 'a';
            hash ^= hash * 43 + 'n';
            hash ^= hash * 43 + 'd';
            break;

        case kLBracket:
            // we found something bracketed, e.g. (uk) or (2019)
            // so we also check the intermediate hash against the dictionary,
            //
            debugf( "checking: %016lx\n", hash);

            result = findValue( dictionary, hash );
            if (result != NULL )
            {
                return result;
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
        c = hashSeriesMap[ *s++ ];
    }

    debugf( "checking: %016lx\n", hash);

    result = findValue( dictionary, hash );

    debugf( "result %s\n", result != NULL ? result : "<not found>" );
    return result;
}

void addSeasonEpisode( tDictionary * dictionary, unsigned int season, unsigned int episode )
{
    char  temp[50];

    snprintf( temp, sizeof(temp), "%02u", season );
    addParam( dictionary, kHashSeason, temp );
    if ( season == 0 )
    {
        addParam( dictionary, kHashSeasonFolder, "Specials" );
    }
    else
    {
        snprintf( temp, sizeof(temp), "Season %02u", season );
        addParam( dictionary, kHashSeasonFolder, temp );
    }

    snprintf( temp, sizeof(temp), "%02u", episode );
    addParam( dictionary, kHashEpisode, temp );
}

int storeParam( tDictionary *dictionary, tHash hash, char * value )
{
    // struct tm firstAired;
    // struct tm dateRecorded;
    // struct tm year;
    unsigned int season  = 0;
    unsigned int episode = 0;

    switch (hash)
    {
    case kHashSnnEnn:   // we found 'SnnEnn' or
    case kHashSyyyyEnn: // SyyyyEnnn
    case kHashSnnEn:    // SnnEn
    case kHashSnEnn:    // SnEnn
    case kHashSnEn:     // SnEn
        debugf("SnnEnn = %s\n", value);
        sscanf( value, "%*1c%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kHashnXnn:
    case kHashnnXnn:
        debugf("nnXnn = %s\n", value);
        sscanf( value, "%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kHashOneDash:
        // debugf("one dash = %s\n", value);
        break;

    case kHashDate:
        debugf("yyyy-mm-dd = %s\n", value);
#if 1
        addParam( dictionary, kHashFirstAired, value );
#else
        strptime( value, "%Y-%m-%d", &firstAired );
        strftime( (char * restrict)temp, sizeof(temp), "%x", &firstAired );
        debugf("first aired: %s\n", temp);
        addParam( dictionary, kHashFirstAired, temp );
#endif
        break;

    case kHashDateTime:
        debugf("yyyy-mm-dd-hhmm = %s\n", value);
#if 1
        addParam( dictionary, kHashDateRecorded, value );
#else
        strptime( value, "%Y-%m-%d-%H%M", &dateRecorded);
                strftime( (char * restrict)temp, sizeof(temp), "%x %X", &dateRecorded );
                debugf("recorded: %s\n", temp);
                addParam( dictionary, kHashDateRecorded, temp );
#endif
        break;

    default:
        break;
    }
    return 0;
}

int parseName( tDictionary *dictionary, char *name )
{
    int histogram[256];

    memset( histogram, 0, sizeof(histogram));
    for ( unsigned char * s = (unsigned char *)name; *s != '\0'; ++s )
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


    char * temp = strdup( name ); // copy it, because we'll modify it in place as we go
    if ( temp != NULL )
    {
        char seenSeries = 0;
        char * start = temp;
        char * end = temp;
        char * prevSep = &temp[ strlen( temp ) ]; // initially point at the last null
        tHash  hash = 0;

        unsigned char c;
        do { // has to be a 'do...while' so code inside the loop also sees the terminating null
            c = *end;
            if ( c != sep && c != '\0' )
            {
                // calculate the hash character-by-character
                if ( hashCharMap[ c ] != kIgnore ) /* we ignore some characters when calculating the hash */
                {
                    hash ^= hash * 43 + hashCharMap[ c ];
                }
            }
            else // we reached a separator, or the end of the string
            {
                *end = '\0'; // terminate the token (will be undone later if no hash match)
                debugf( "token: \'%s\' = %016lx\n", prevSep != NULL ? &prevSep[1] : start, hash );
                debugf( "run: \'%s\'\n", start );

                // check to see if the token has a hash/pattern we recognize
                switch (hash)
                {
                    // hashes of the patterns we're looking for.
                case kHashSnnEnn:   // SnnEnn
                case kHashSyyyyEnn: // SnnnnEnn
                case kHashSnnEn:    // SnnEn
                case kHashSnEnn:    // SnEnn
                case kHashSnEn:     // SnEn
                case kHashnXnn:     // nXnn
                case kHashnnXnn:    // nnXnn
                case kHashOneDash:  // -
                case kHashDate:     // yyyy-mm-dd
                case kHashDateTime: // yyyy-mm-dd-hhmm
                    if ( prevSep != NULL )
                    {
                        // first store the run of unmatched tokens into a param
                        *prevSep = '\0';

                        if (seenSeries)
                        { // second (and subsequent) unmatched runs are assumed to be episode title
                            debugf( "store title: \'%s\'\n", start );
                            addParam( dictionary, kHashTitle, start );
                        }
                        else
                        { // first unmatched run is presumed to be the series
                            debugf( "store series: \'%s\'\n", start );
                            addParam( dictionary, kHashSeries, start );
                            seenSeries = 1;
                        }

                        // now point at the beginning of the matched hash, which is just past prevSep
                        start = &prevSep[1];
                        prevSep = NULL;
                    }

                    debugf( "store: \'%s\'\n", start );
                    storeParam( dictionary, hash, start );
                    start = &end[1];
                    break;

                default: // not a recognized pattern
                    if ( c == '\0' ) // reached the end of the name string
                    {
                        if (seenSeries)
                        { // second (and subsequent) unmatched runs are assumed to be episode title
                            debugf( "store title 0: \'%s\'\n", start );
                            addParam( dictionary, kHashTitle, start );
                        }
                        else
                        { // first unmatched run is presumed to be the series
                            debugf( "store series 0: \'%s\'\n", start );
                            addParam( dictionary, kHashSeries, start );
                            seenSeries = 1;
                        }
                    }
                    else
                    {   // not at the end of the string, so undo the null written just before switch
                        *end = ' ';
                    }
                    prevSep = end;
                    break;
                }
                hash = 0; // reset to begin hashing the next token

            } // endif seperator
            ++end;
        } while ( c != '\0' );
    }

    free( temp );

    return 0;
}

/*
 * carve up the path into directory path, basename and extension
 * then pass basename onto parseName() to be processed
 */
int parsePath( tDictionary *dictionary, char *path )
{
    int result = 0;

    addParam( dictionary, kHashSource, path );

    char *lastPeriod = strrchr( path, '.' );
    if ( lastPeriod != NULL ) {
        addParam( dictionary, kHashExtension, lastPeriod );
    }
    else
    {
        lastPeriod = path + strlen( path );
    }

    char *lastSlash = strrchr( path, '/' );
    if ( lastSlash != NULL )
    {
        char *p = strndup( path, lastSlash - path );
        addParam( dictionary, kHashPath, p );
        free( p );

        ++lastSlash;
    }
    else
    {
        lastSlash = path; // no directories prefixed
    }

    char *basename = strndup( lastSlash, lastPeriod - lastSlash );
    addParam( dictionary, kHashBasename, basename );
    parseName( dictionary, basename );
    free( basename );

    return result;
}

char *buildString( tDictionary *mainDict, tDictionary *fileDict, char *template )
{
    const char *t = template;
    char *result = NULL;
    char *s;    // pointer into the returned string

    result = calloc(1, 32768);
    s = result;

    if ( s != NULL )
    {
        unsigned char c = *t++; // unsigned because it is used as an array subscript when calculating the hash
        while ( c != '\0' )
        {
            unsigned long hash;
            const char * k;

            switch (c)
            {
            case '{':   // start of keyword
                    k = t; // remember where the keyword starts
                    debugf( "[%s]\n", k );
                    c = *t++;

                    // scan the keyword and generate its hash
                    hash = 0;

                    while ( c != '\0' && c != '}' && c != '?' )
                    {
                        if ( hashCharMap[ c ] != kIgnore ) /* we ignore some characters when calculating the hash */
                        {
                            hash ^= hash * 43 + hashCharMap[ c ];
                        }
                        c = *t++;
                    }

                    if ( hash != kHashTemplate ) // don't want to expand a {template} keyword in a template
                    {
                        char * value = findValue( fileDict, hash );

                        if ( value == NULL ) // not in the file dictionary, try the main dictionary
                        {
                            value = findValue( mainDict, hash );
                        }

                        if ( value == NULL ) // not in the main dictionary either, check for an environment variable
                        {
                            char * envkey = strndup( k, t - k - 1 );
                            value = getenv( envkey );
                            // debugf( "env=\"%s\", value=\"%s\"\n", envkey, value );
                            free( envkey );
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

int main( int argc, char * argv[] )
{
    int result = 0;
    int cnt = argc;
    char *configPath = NULL;

    tDictionary * mainDict = createDictionary();

    int i = 1;
    int k = 1;
    while ( i < argc && result == 0 )
    {
        debugf( "i = %d, k = %d, cnt = %d, \'%s\'\n", i, k, cnt, argv[i] );

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
                ++i;

                switch ( option )
                {
                case 'c':   // config file
                    // swallow the option, we'll swallow the argument at the bottom of the switch
                    --cnt;
                    configPath = argv[ i ];
                    break;

                case 'd':   // destination
                    --cnt;
                    addParam( mainDict, kHashDestination, argv[ i ] );
                    break;

                case 't':   // template
                    --cnt;
                    addParam( mainDict, kHashTemplate, argv[ i ] );
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
        ++i;
    }

    debugf( "config path = \'%s\'\n", configPath );

    /* ToDo: parse config file */

    fprintf( stderr, "main dictionary\n");
    printDictionary( mainDict );

    tDictionary *destSeriesDict = createDictionary();

    char * destination = findValue( mainDict, kHashDestination );

    if ( destination == NULL )
    {
        destroyDictionary( destSeriesDict );
        fprintf( stderr, "### Error: no destination defined.\n" );
        result = -3;
    }
    else
    {
        // fill the dictionary with hashes of the directory names in the destination
        buildSeriesDictionary( destSeriesDict, destination );
    }


    char * template = findValue( mainDict, kHashTemplate );
    if ( template == NULL )
    {
        fprintf( stderr, "### Error: no template defined.\n" );
        result = -2;
    }
    else
    {
        debugf( "template = \'%s\'\n", template );


        for ( int i = 1; i < cnt && result == 0; ++i )
        {
            tDictionary *fileDict = createDictionary();

            debugf( "%d: \'%s\'\n", i, argv[ i ] );

            parsePath( fileDict, argv[ i ] );

            char * series = findValue( fileDict, kHashSeries );
            if ( series != NULL )
            {
                char * destSeries = lookupSeries( destSeriesDict, series );
                if ( destSeries != NULL )
                {
                    addParam( fileDict, kHashDestSeries, destSeries );
                }
            }

            fprintf( stderr, "file dictionary\n");
            printDictionary( fileDict );

            char * string = buildString( mainDict, fileDict, template );
            printf( "%s%c", string, '\n' );
            free( string );

            destroyDictionary( fileDict );
        }
    }
    destroyDictionary( mainDict );

    return result;
}
