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

#include "chandvr2plex.h"

typedef enum
{
    kIgnore   = ' ', // do not start at zero - possible confusion with string termination
    kNumber   = '0',
    kLBracket = '(',
    kRBracket = ')'
} tCharClass;

typedef unsigned long tHash;

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
    ',',                '-',                kIgnore, /* . */    '/',
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
    const char * name;
} tDictionary;

/**
 * trim any trailing whitespace from the end of the string
 *
 * @param line
 */
void trimTrailingWhitespace(char *line)
{
    char * t = line;

    while (*t != '\0') { t++; }
    t--;
    while (t > line && isspace(*t)) { t--; }
    t++;
    *t = '\0';
}

tDictionary * createDictionary( const char * name )
{
    tDictionary * result = (tDictionary *)calloc( 1, sizeof(tDictionary) );
    result->name = name;
    return result;
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
        debugf( "...%s dictionary...\n", dictionary->name);

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

#if 0
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
#endif

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
#define kPattern_SnnEnn     0x00000003f9b381c4  // SnnEnn
#define kPattern_SyyyyEnn   0x00001ccd9c944b04  // SyyyyEnn
#define kPattern_SnnEn      0x00000000176a4622  // SnnEn
#define kPattern_SnEnn      0x00000000176e12d4  // SnEnn
#define kPattern_SnEn       0x00000000008e24fa  // SnEn
#define kPattern_nXnn       0x0000000000410fb0  // nXnn
#define kPattern_nnXnn      0x0000000009e517b0  // nnXnn
#define kPattern_OneDash    0x000000000000002d  // -
#define kPattern_Date       0x0055006b8132df24  // yyyy-mm-dd
#define kPattern_DateTime   0x20ef704b1d973420  // yyyy-mm-dd-hhmm

/* keywords in the template */
#define kKeyBasename        0x00001770bd72d401
#define kKeyDateRecorded    0x46b649d0032996fe
#define kKeyDestination     0x1f8c5d6e23059b0c
#define kKeyDestSeries      0x00b9cdb100649b5f
#define kKeyEpisode         0x00000099d3300841
#define kKeyExtension       0x00045d732bb4c26c
#define kKeyFirstAired      0x00b3fed4e2899d3e
#define kKeyPath            0x00000000008bc56c
#define kKeySeason          0x000000043a32a26c
#define kKeySeasonFolder    0x26b2db9d04411e4c
#define kKeySeries          0x00000003d1109b5f
#define kKeySource          0x0000000416730735
#define kKeyTemplate        0x00001c70c5df6271
#define kKeyTitle           0x000000001857f9b5
#define kKeyExecute         0x0000009ac9ecdcb1
#define kKeyStdin           0x000000001825d07b
#define kKeyNullTermination 0xd8f661f7221f0b0c


/*
 * this uses the 'key' hash table, since comparing series
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
    unsigned char  c = hashKey[ *s++ ];

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
        c = hashKey[ *s++ ];
    }

    // also add the hash of the full string, including any trailing bracketed stuff
    addParam( dictionary, result, series );
}

static int scanDirFilter( const struct dirent * entry)
{
    int result = 0;

    result = ( entry->d_name[0] != '.' && entry->d_type == DT_DIR );

    // debugf( "%s, 0x%x, %d\n", entry->d_name, entry->d_type, result );
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

    /* printDictionary( dictionary ); */

    return 0;
}

char * lookupSeries( tDictionary * dictionary, char * series )
{
    char * result = NULL;
    tHash hash = 0;
    unsigned char *s = (unsigned char *) series;
    unsigned char  c = hashKey[ *s++ ];


    while ( c != '\0' )
    {
        switch ( c )
        {
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
        c = hashKey[ *s++ ];
    }

    debugf( "checking: %016lx\n", hash);

    result = findValue( dictionary, hash );

    debugf( "match: %s\n", result != NULL ? result : "<not found>" );
    return result;
}

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

int storeParam( tDictionary *dictionary, tHash hash, char * value )
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
        debugf("SnnEnn: %s\n", value);
        sscanf( value, "%*1c%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kPattern_nXnn:
    case kPattern_nnXnn:
        debugf("nnXnn: %s\n", value);
        sscanf( value, "%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( dictionary, season, episode );
        break;

    case kPattern_OneDash:
        // debugf("one dash = %s\n", value);
        break;

    case kPattern_Date:
        debugf("yyyy-mm-dd: %s\n", value);
#if 1
        addParam( dictionary, kKeyFirstAired, value );
#else
        strptime( value, "%Y-%m-%d", &firstAired );
        strftime( (char * restrict)temp, sizeof(temp), "%x", &firstAired );
        debugf("first aired: %s\n", temp);
        addParam( dictionary, kKeyFirstAired, temp );
#endif
        break;

    case kPattern_DateTime:
        debugf("yyyy-mm-dd-hhmm: %s\n", value);
#if 1
        addParam( dictionary, kKeyDateRecorded, value );
#else
        strptime( value, "%Y-%m-%d-%H%M", &dateRecorded);
        strftime( (char * restrict)temp, sizeof(temp), "%x %X", &dateRecorded );
        debugf("recorded: %s\n", temp);
        addParam( dictionary, kKeyDateRecorded, temp );
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
    if ( histogram['.'] > histogram[sep] )
        sep = '.';
    if ( histogram['_'] > histogram[sep] )
        sep = '_';
    if ( histogram['-'] > histogram[sep] )
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
                if ( hashPattern[ c ] != kIgnore ) /* we ignore some characters when calculating the hash */
                {
                    hash ^= hash * 43 + hashPattern[ c ];
                }
            }
            else // we reached a separator, or the end of the string
            {
                *end = '\0'; // terminate the token (will be undone later if no hash match)
                /* debugf( "token: \'%s\' = %016lx\n", prevSep != NULL ? &prevSep[1] : start, hash ); */
                /* debugf( "run: \'%s\'\n", start ); */

                // check to see if the token has a hash/pattern we recognize
                switch (hash)
                {
                    // hashes of the patterns we're looking for.
                case kPattern_SnnEnn:   // SnnEnn
                case kPattern_SyyyyEnn: // SnnnnEnn
                case kPattern_SnnEn:    // SnnEn
                case kPattern_SnEnn:    // SnEnn
                case kPattern_SnEn:     // SnEn
                case kPattern_nXnn:     // nXnn
                case kPattern_nnXnn:    // nnXnn
                case kPattern_OneDash:  // -
                case kPattern_Date:     // yyyy-mm-dd
                case kPattern_DateTime: // yyyy-mm-dd-hhmm
                    if ( prevSep != NULL )
                    {
                        // first store the run of unmatched tokens into a param
                        *prevSep = '\0';

                        if (seenSeries)
                        { // second (and subsequent) unmatched runs are assumed to be episode title
                            debugf( "title: \'%s\'\n", start );
                            addParam( dictionary, kKeyTitle, start );
                        }
                        else
                        { // first unmatched run is presumed to be the series
                            debugf( "series: \'%s\'\n", start );
                            addParam( dictionary, kKeySeries, start );
                            seenSeries = 1;
                        }

                        // now point at the beginning of the matched hash, which is just past prevSep
                        start = &prevSep[1];
                        prevSep = NULL;
                    }

                    /* debugf( "store: \'%s\'\n", start ); */
                    storeParam( dictionary, hash, start );
                    start = &end[1];
                    break;

                default: // not a recognized pattern
                    if ( c == '\0' ) // reached the end of the name string
                    {
                        if (seenSeries)
                        { // second (and subsequent) unmatched runs are assumed to be episode title
                            debugf( "store title 0: \'%s\'\n", start );
                            addParam( dictionary, kKeyTitle, start );
                        }
                        else
                        { // first unmatched run is presumed to be the series
                            debugf( "store series 0: \'%s\'\n", start );
                            addParam( dictionary, kKeySeries, start );
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

    addParam( dictionary, kKeySource, path );

    char *lastPeriod = strrchr( path, '.' );
    if ( lastPeriod != NULL )
    {
        addParam( dictionary, kKeyExtension, lastPeriod );
    }
    else
    {
        lastPeriod = path + strlen( path );
    }

    char *lastSlash = strrchr( path, '/' );
    if ( lastSlash != NULL )
    {
        char *p = strndup( path, lastSlash - path );
        addParam( dictionary, kKeyPath, p );
        free( p );

        ++lastSlash;
    }
    else
    {
        lastSlash = path; // no directories prefixed
    }

    char * basename = strndup( lastSlash, lastPeriod - lastSlash );
    addParam( dictionary, kKeyBasename, basename );
    parseName( dictionary, basename );
    free( basename );

    return result;
}

char *buildString( tDictionary *mainDict, tDictionary *fileDict, const char *template )
{
    char * result = NULL;
    const char * t = template;
    char * s;    // pointer into the returned string

    result = calloc( 1, 32768 );
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
                    char * tmpStr = strndup( k, t - k - 1 );
                    debugf( "key \'%s\' = 0x%016lx\n", tmpStr, hash );
                    free( tmpStr );
#endif
                    if ( hash != kKeyTemplate ) // don't want to expand a {template} keyword in a template
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

int parseConfigFile( tDictionary * dictionary, char * path )
{
    int  result  = 0;
    FILE *file;
    char *buffer = malloc( 4096 ); // 4K seems like plenty

    debugf( "config file: \"%s\"\n", path );
    file = fopen( path, "r" );
    if ( file == NULL)
    {
        fprintf( stderr, "### Error: Unable to open config file \'%s\': ", path );
        perror(NULL);
        return -5;
    }
    else
    {
        while ( fgets( buffer, 4096, file ) != NULL)
        {
            trimTrailingWhitespace( buffer );
            debugf( "line: \"%s\"\n", buffer );

            tHash hash = 0;
            char  *s   = buffer;
            while ( isspace( *s ))
            {
                s++;
            }

            unsigned char c = (unsigned char) *s++;
            if ( c != '\0' )
            {
                while ( c != '\0' && c != '=' )
                {
                    if ( c != kIgnore )
                    {
                        hash ^= hash * 43 + hashKey[ c ];
                    }
                    c = (unsigned char) *s++;
                }

                if ( c == '=' )
                {
                    // trim whitespace from the beginning of the value
                    while ( isspace( *s ) )
                    {
                        s++;
                    }

                    char *e = s;
                    char *p = s;
                    while ( *p != '\0' )
                    {
                        if ( !isspace( *p ) )
                        {
                            e = p; // remember the location of the most recent non-whitespace character we've seen
                        }
                        p++;
                    }
                    // e should now point at the last non-whitespace character in the string
                    e[ 1 ] = '\0'; // trim off any trailing whitespace at the end of the string - including the LF

                }
                debugf( "hash = 0x%016lx, value = \'%s\'\n", hash, s );
                addParam( dictionary, hash, s );
            }
        }
        free( buffer );
        fclose( file );
    }
    return result;
}

/*
 * look for config files to process. First, look in /etc/<argv[0]>.conf then in ~/.config/<argv[0]>.conf,
 * and finally the file passed as a -c parameter, if any, then any parameters on the command line (except -c)
 * Where a parameter occurs more than once in a dictionary, the most recent definition 'wins'
 */

int parseConfig( tDictionary * dictionary, char * path, char *myName )
{
    int result = 0;
    char temp[PATH_MAX];

    snprintf( temp, sizeof( temp ), "/etc/%s.conf", myName );
    debugf( "/etc path: \"%s\"\n", temp );
    if ( eaccess( temp, R_OK ) == 0 ) // only attempt to parse it if there's something there
    {
        result = parseConfigFile( dictionary, temp );
    }

    if ( result == 0 )
    {
        const char * home = getenv("HOME");
        if ( home == NULL)
        {
            home = getpwuid(getuid())->pw_dir;
        }
        if ( home != NULL )
        {
            snprintf( temp, sizeof( temp ), "%s/.config/%s.conf", home, myName );
            debugf( "~ path: \"%s\"\n", temp );

            switch ( eaccess( temp, R_OK ) )   // only attempt to parse it if there's something there
            {
            case 0:
                result = parseConfigFile( dictionary, temp );
                break;

            case ENOENT:
                break;

            default:
                fprintf( stderr, "### Error: Unable to read config file \'%s\': ", temp );
                perror( NULL );
                break;
            }
        }
    }

    if ( result == 0 && path != NULL )
    {
        snprintf( temp, sizeof( temp ), "%s/%s.conf", path, myName );
        debugf( "-c path: %s\n", temp );

        switch ( eaccess( temp, R_OK ) )  // only attempt to parse it if there's something there
        {
        case 0:
            result = parseConfigFile( dictionary, temp );
            break;

        default:
            fprintf( stderr, "### Error: Unable to read config file \'%s\': ", temp );
            perror( NULL );
            result = -5;
            break;
        }
    }

    return result;
}

int processFile( tDictionary * mainDict, tDictionary * destSeriesDict, char *path)
{
    int result = 0;

    char * template = findValue( mainDict, kKeyTemplate );
    if ( template == NULL )
    {
        fprintf( stderr, "### Error: no template defined.\n" );
        result = -2;
    }
    else
    {
        debugf( "template = \'%s\'\n", template );

        tDictionary *fileDict = createDictionary( "File" );
        if ( fileDict != NULL )
        {
            parsePath( fileDict, path );

            char * series = findValue( fileDict, kKeySeries );
            if ( series != NULL )
            {
                char * destSeries = lookupSeries( destSeriesDict, series );

                if ( destSeries == NULL )
                {
                    addParam( fileDict, kKeyDestSeries, series );
                }
                else
                {
                    addParam( fileDict, kKeyDestSeries, destSeries );
                }

            }

            printDictionary( fileDict );

            char * string = buildString( mainDict, fileDict, template );
            char * exec   = findValue( mainDict, kKeyExecute );
            if ( exec != NULL )
            {
                result = system(string);
            }
            else
            {
                printf( "%s%c", string, '\n' );
                result = 0;
            }
            free( string );

            destroyDictionary( fileDict );
        }
    }
    return result;
}

int main( int argc, char * argv[] )
{
    int  result       = 0;
    int  cnt          = argc;
    char * configPath = NULL;

    tDictionary * mainDict = createDictionary( "Main" );

    char * myName = basename( strdup( argv[0] ) ); // posix flavor of basename modifies its argument

    int k = 1;
    for ( int i = 1; i < argc && result == 0; i++ )
    {
        debugf( "1: i = %d, k = %d, cnt = %d, \'%s\'\n", i, k, cnt, argv[ i ] );

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
        free( configPath );
        configPath = NULL;
    }

    k = 1;
    for ( int i = 1; i < argc && result == 0; i++ )
    {
        debugf( "2: i = %d, k = %d, cnt = %d, \'%s\'\n", i, k, cnt, argv[i] );

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

    tDictionary *destSeriesDict = createDictionary( "Series" );

    char * destination = findValue( mainDict, kKeyDestination );

    if ( destination == NULL )
    {
        fprintf( stderr, "### Error: no destination defined.\n" );
        result = -3;
    }
    else
    {
        // fill the dictionary with hashes of the directory names in the destination
        buildSeriesDictionary( destSeriesDict, destination );
    }


    for ( int i = 1; i < argc && result == 0; ++i )
    {
        debugf( "%d: \'%s\'\n", i, argv[ i ] );
        processFile( mainDict, destSeriesDict, argv[i] );
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
                    debugf( "null: %s\n", line );
                    processFile( mainDict, destSeriesDict, line );

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
                debugf("eol: %s\n", line);
                processFile(mainDict, destSeriesDict, line);
            }
        }
    }

    // all done, clean up.
    destroyDictionary( mainDict );
    destroyDictionary( destSeriesDict );

    return result;
}
