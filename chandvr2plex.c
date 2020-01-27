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


/*  hashes for patterns we are scanning for in the filename
    this hash table is used to generate hashes used to match patterns.
    it maps all digits to the same value, maps uppercase letters to
    lowercase, and ignores several characters completely.
 */
#include "patterns.h"

/*
   Hashes for the keywords/parameter names in the template. This hash table is also
   used for series names.

   Periods are ignored, because the trailing one is often omitted of series like
   "S.W.A.T." and "Marvel's Agents of S.H.I.E.L.D.". By ignoring periods,
   "S.W.A.T.", "S.W.A.T" and "SWAT" will all result in the same hash value.

   Since periods may also be used as a separator, we have to treat ' ' and '_' as
   equivalent, or the hash for a space-separated name won't match the hash of a
   period- or underscore-seperated one.

   In other words, ' ', '_' and '.' do not contribute to the series hash. Similarly,
   apostrophes are also often omitted ("Marvel's" becomes "Marvels"), so it is
   similarly ignored when generating a hash, along with '?' (e.g. "Whose Line Is It
   Anyway?") and '!' ("I'm a Celebrity...Get Me Out of Here!").

   "Marvel's Agents of S.H.I.E.L.D. (2017)" is perhaps one of the most difficult
   matching examples I've seen in the wild. There are so many ways to mangle that.

   ':' is usually converted to '-' or omitted entirely, so ignore those, too.

   Left and right brackets are also mapped to be equivalent, e.g. [2017] has the
   same hash as (2017).
 */
#include "keywords.h"

string gMyName;
int gDebugLevel = 3;
unsigned int gNextYear = 1895;

tDictionary * gMainDict;
tDictionary * gPathDict;
tDictionary * gFileDict;
tDictionary * gSeriesDict;

string gCachedPath   = NULL;
string gCachedSeries = NULL;

typedef struct sToken
{
	struct sToken * next;
	string        start;
	string        end;
	tHash         hash;
	unsigned char seperator;
} tToken;

tToken gTokenList;

/**
 * trim any trailing whitespace from the end of the string
 *
 * @param line	line to be trimmed
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

string lookupHash(tHash hash)
{
	tKeywordHashMapping * keywordMap = KeywordHashLookup;

	while ( keywordMap->key != 0 )
	{
		if ( hash == keywordMap->key )
		{
			return keywordMap->label;
		}
		keywordMap++;
	}

	tPatternHashMapping * patternMap = PatternHashLookup;

	while ( patternMap->key != 0 )
	{
		if ( hash == patternMap->key )
		{
			return patternMap->label;
		}
		patternMap++;
	}

	return "<unknown>";
}

/**
 * @brief look in the three dictionaries for the first occurance of a hash value
 * @param hash
 * @return
 */
string findParam( tHash hash )
{
	string result;

	result = findValue( gFileDict, hash );
	if ( result == NULL )
	{
		result = findValue( gPathDict, hash );
	}
	if ( result == NULL )
	{
		result = findValue( gMainDict, hash );
	}
	return result;
}

/**
   Hashes the 'series' using the 'keyword' hash table, since comparing series names needs
   slightly different logic than scanning for patterns. Separators (spaces, periods,
   underscores) are ignored completely. As are \', !, amd ?, since those are frequently
   omitted. Upper case letters are mapped to lower case since those are also very
   inconsistent (no UTF-8 handling yet, though). and '&' is expanded to 'and' in the
   hash, so both forms will hash to the same value.

   Since a series name may or may not be suffixed by a year or country surrounded
   by brackets (e.g. (2019) or (US)). So a hash is added whenever a left bracket
   is encountered, so the hash for 'Some Series' and 'Some Series (2019)' are both
   stored in the series dictionary, so there will be a hash available to match
   either with or without the suffix.
 */
void addSeries( string series )
{
    tHash result = 0;
    unsigned char * s = (unsigned char *)series;
    unsigned char   c;

    do {
        c = kKeywordMap[ *s++ ];
        switch ( c )
        {
        case '\0':
            break;

            // we hash the '&' character as if 'and' was used. so both forms generate the same hash
            // e.g. the hash of 'Will & Grace' will match the hash of 'Will and Grace'
        case '&':
            result = fKeywordHashChar( result, 'a' );
            result = fKeywordHashChar( result, 'n' );
            result = fKeywordHashChar( result, 'd' );
            break;

        case kKeywordLBracket:
            // we found something bracketed, e.g. (uk) or (2019), so we also add the
            // intermediate hash to the dictionary, before we hash the bracketed content.
            // Then if we hash the same series with the year omitted, for example, will
            // still match something. Though we can't do much about a file that omits a
            // a year or country, e.g. 'MacGyver' instead of 'MacGyver (2016)', or
            // 'Hell's Kitchen' instead of 'Hell's Kitchen (US)'
            //
            // Note: if there are multiple left brackets encountered, there will be
            // multiple intermediate hashes added.

            addParam( gSeriesDict, result, series );
            result = fKeywordHashChar( result, c );
            break;

        default:
            if ( c != kKeywordSeparator )
            {
                result = fKeywordHashChar( result, c );
            }
            break;
        }
    } while ( c != '\0' );

    // also add the hash of the full string, including any trailing bracketed stuff
    addParam( gSeriesDict, result, series );
}

static int scanDirFilter( const struct dirent * entry)
{
    int result = 0;

    result = ( entry->d_name[0] != '.' && entry->d_type == DT_DIR );

    // debugf( 3, "%s, 0x%x, %d\n", entry->d_name, entry->d_type, result );
    return result;
}

int buildSeriesDictionary( string path )
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
        addSeries( namelist[ i ]->d_name );
        free( namelist[ i ] );
    }
    free(namelist);

    /* printDictionary( dictionary ); */

    return 0;
}

void addSeasonEpisode( unsigned int season, unsigned int episode )
{
    char  temp[50];

    snprintf( temp, sizeof(temp), "%02u", season );
    addParam( gFileDict, kKeywordSeason, temp );
    if ( season == 0 || episode == 0 )
    {
	    addParam( gFileDict, kKeywordSeasonFolder, "Specials" );
    }
    else
    {
        snprintf( temp, sizeof(temp), "Season %02u", season );
	    addParam( gFileDict, kKeywordSeasonFolder, temp );
    }

	snprintf( temp, sizeof(temp), "%02u", episode );
    addParam( gFileDict, kKeywordEpisode, temp );
}

void storeSeries( string series )
{
    string result = series;
    string ptr, end;
    tHash hash;
    unsigned char c;

    ptr  = series;
    hash = 0;

    addParam( gFileDict, kKeywordSeries, series );

    // regenerate the hash incrementally, checking at each separator.
    // remember the longest match, i.e. keep looking until the end of the string
    do {
        c = kKeywordMap[ (unsigned char)*ptr ];
        switch ( c )
        {
        case kKeywordSeparator:
        case '\0':
        	/* let's see if we have a match */
            debugf( 3, "checking: 0x%016lx\n", hash );

            string match = findValue( gSeriesDict, hash );
            if ( match != NULL)
            {
                result = match;
                debugf( 3, "matched %s\n", result );
                end = ptr;
            }
            break;

        case '&':
            hash = fPatternHashChar( hash, 'a' );
            hash = fPatternHashChar( hash, 'n' );
            hash = fPatternHashChar( hash, 'd' );
            break;

        default:
            hash = fPatternHashChar( hash, c );
            break;
        };
        ptr++;
    } while ( c != '\0' );

    if ( result != series )
    {
        if ( *end != '\0' )
        {
            /* if the run is longer than the match with the series name,
               then store the trailing remnant as the episode title */
            addParam( gFileDict, kKeywordTitle, (string) end + 1 );
	        *(char *) end = '\0';
        }
    }
	addParam( gFileDict, kKeywordDestSeries, result );
}

int storeToken( tHash hash, string value )
{
    unsigned int season  = 0;
    unsigned int episode = 0;
    unsigned int year    = 0;
    char temp[20];
    string seriesName;

    switch (hash)
    {
    case kPatternSnnEnn:   // we found 'SnnEnn' or
    case kPatternSyyyyEnn: // SyyyyEnnn
    case kPatternSnnEn:    // SnnEn
    case kPatternSnEnn:    // SnEnn
    case kPatternSnEn:     // SnEn
        debugf( 3,"SnnEnn: %s\n", value);
        sscanf( value, "%*1c%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( season, episode );
        break;

    case kPatternEnnn:
        debugf( 3,"Ennn: %s\n", value);
        sscanf( value, "%*1c%u", &episode ); // ignore characters since we don't know their case
        season = episode / 100;
        episode %= 100;
        addSeasonEpisode( season, episode );
        break;

    case kPatternEnnnn:
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
        addSeasonEpisode( season, episode );
        break;

    case kPatternnXnn:
    case kPatternnnXnn:
        debugf( 3, "nnXnn: %s\n", value);
        sscanf( value, "%u%*1c%u", &season, &episode ); // ignore characters since we don't know their case
        addSeasonEpisode( season, episode );
        break;

    case kPatternYear:
	    sscanf( value, "%*1c%u%*1c", &year ); // ignore characters since we don't know their case
        if ( 1890 < year && year <= gNextYear )
        {
            snprintf( temp, sizeof( temp ), "%u", year );
            addParam( gFileDict, kKeywordYear, temp );
        }
	    debugf( 3, "year: %u\n", year );
	    break;

    case kPatternCountryUSA:
    	addParam( gFileDict, kKeywordCountry, "USA" );
    	break;

    case kPatternCountryUS:
	    addParam( gFileDict, kKeywordCountry, "US" );
	    break;

    case kPatternCountryUK:
	    addParam( gFileDict, kKeywordCountry, "UK" );
	    break;

    case kPatternNoMatch:
        seriesName = findParam( kKeywordSeries );
        if ( seriesName == NULL )
        {
            debugf( 3, "series: %s\n", value );
            storeSeries( value );
        }
        else
        {
            debugf( 3, "title: %s\n", value );
            addParam( gFileDict, kKeywordTitle, value );
        }
        break;

    // kPatternTwoDigits:
    // kPatternFourDigits:
    // kPatternSixDigits:
    // kPatternEightDigits:
    default:
        break;
    }
    return 0;
}

tHash checkHash( tHash hash)
{
    switch (hash)
    {
    case kPatternSnnEnn:       // SnnEnn
    case kPatternSyyyyEnn:     // SnnnnEnn
    case kPatternSnnEn:        // SnnEn
    case kPatternSnEnn:        // SnEnn
    case kPatternSnEn:         // SnEn
    case kPatternEnnn:         // Ennn
    case kPatternEnnnn:        // Ennnn
    case kPatternnXnn:         // nXnn
    case kPatternnnXnn:        // nnXnn
    case kPatternTwoDigits:    // nn
    case kPatternFourDigits:   // nnnn
    case kPatternSixDigits:    // nnnnnn
    case kPatternEightDigits:  // nnnnnnnn
    case kPatternCountryUSA:   // (USA)
    case kPatternCountryUS:    // (US)
    case kPatternCountryUK:    // (UK)
    case kPatternYear:         // (nnnn)
    	break;

    default:
        hash = kPatternNoMatch;
        break;
    }
    return hash;
}

void tokenizeName( string originalName )
{
	gTokenList.next = NULL;

	string name = strdup( originalName ); // copy it, because we'll terminate strings in place as we go

	if ( name != NULL)
	{
		unsigned char c;

		string start = name;
		string ptr   = start;
		tHash  hash  = 0;

		tToken * token = &gTokenList;

		do {
			c = kPatternMap[ *(unsigned char *)ptr ];
			switch ( c )
			{
			case kPatternSeperator:
			case '\0':
				// reached the end of a token
				token->next = calloc( 1, sizeof(tToken) );
				token = token->next;
				if ( token != NULL )
				{
					token->hash      = checkHash( hash );
					token->start     = start;
					token->end       = ptr;
					token->seperator = *ptr;
					*(char *)ptr = '\0';
				}
				// only prepare for the next run if we're nt at the end of the string
				if ( c != '\0' )
				{
					// skip over a run of kPatternSeperator, if present (e.g. ' - ')
					do { ptr++; } while ( kPatternMap[ *(unsigned char *)ptr ] == kPatternSeperator );
					start = ptr;
					hash = 0;
				}
				break;

			case '&':
				hash = fPatternHashChar( hash, 'a' );
				hash = fPatternHashChar( hash, 'n' );
				hash = fPatternHashChar( hash, 'd' );
				ptr++;
				break;

			default:
				hash = fPatternHashChar( hash, c );
				ptr++;
				break;
			};
		} while ( c != '\0' );

		token = gTokenList.next;
		while ( token != NULL )
		{
			debugf( 3, "token: \'%s\', \'%s\' (%c)\n", lookupHash( token->hash ), token->start, token->seperator );
			token = token->next;
		}
	}
}

void freeTokenList( void )
{
    tToken *nextToken;
    tToken * token = gTokenList.next;
    gTokenList.next = NULL;
    while ( token != NULL )
    {
        nextToken = token->next;
        free( token );
        token = nextToken;
    }
}

/*
 * Channels DVR:
 *   air date: yyyy-mm-dd
 *   recorded: yyyy-mm-dd-hhss
 * TVMosaic
 *   recorded: hhss-yyyymmdd
 *
 */
void mergeDigits( void )
{
    tToken * token[4];

    token[0] = gTokenList.next;

    while ( token[0] != NULL)
    {
        token[1] = token[0]->next;
        switch ( token[0]->hash )
        {
            // Channels DVR: YYYY-mm-dd
            //               YYYY-mm-dd-hhss
            //     TVMosaic: HHSS-yyyymmdd
        case kPatternFourDigits:
            if ( token[1] != NULL)
            {
	            token[2] = token[1]->next;

	            switch ( token[1]->hash )
                {
                    // Channels DVR: YYYY-MM-dd
                    //               YYYY-MM-dd-hhss
                case kPatternTwoDigits:
                    if ( token[1]->seperator == '-' && token[2] != NULL)
                    {
                        switch ( token[2]->hash )
                        {
                            // Channels DVR: YYYY-MM-DD
                            //               YYYY-MM-DD-hhss
                        case kPatternTwoDigits:
                            token[3] = token[2]->next;
                            if (token[3] != NULL)
                            {
                                switch ( token[3]->hash )
                                {
                                case kPatternFourDigits:
                                    // ok, looks like we have YYYY-MM-DD-HHSS
                                    token[0]->next = token[3]->next;
                                    *(char *) token[0]->end = '-';
                                    *(char *) token[1]->end = '-';
                                    *(char *) token[2]->end = '-';
                                    token[0]->end  = token[3]->end;
                                    token[0]->hash = kKeywordDateRecorded;
                                    free( token[1] );
                                    free( token[2] );
                                    free( token[3] );
                                    break;

                                default:
                                    // ok, looks like we have YYYY-MM-DD
                                    token[0]->next = token[2]->next;
                                    *(char *) token[0]->end = '-';
                                    *(char *) token[1]->end = '-';
                                    token[0]->end  = token[2]->end;
                                    token[0]->hash = kKeywordFirstAired;
                                    free( token[1] );
                                    free( token[2] );
                                    break;
                                }
                            }
                            break;

                        default:
                            break;
                        }
                    }
                    break;

	                // TVMosaic: HHSS-YYYYMMDD
                case kPatternEightDigits:
	                token[0]->next = token[1]->next;
	                *(char *) token[0]->end = '-';
	                token[0]->end  = token[1]->end;
	                token[0]->hash = kKeywordDateRecorded;
	                free( token[1] );
	                break;

                default:
                    token[0]->hash = kPatternNoMatch;
                    break;
                }
            }
            else
            {
                // last token, therefore four trailing digits, no metapattern
                token[0]->hash = kPatternNoMatch;
            }
            break;

        default:
            // not kPatternFourDigits, ignore it.
            break;
        }
        token[0] = token[0]->next;
    }
}

void mergeNoMatch( void )
{
    tToken * token;
    tToken * nextToken;

	token = gTokenList.next;
    while ( token != NULL)
    {
        nextToken = token->next;
        if ( nextToken != NULL && token->hash == kPatternNoMatch && nextToken->hash == kPatternNoMatch )
        {
            // combine the two kPatternNoMatch tokens
            token->next = nextToken->next;
            *(char *)token->end = ' ';
            token->end = nextToken->end;
            free( nextToken );
        }
        else
        {
            token = token->next;
        }
    }

	/* Some tokens should also be appended as a suffix, while also retaining the token */
	token = gTokenList.next;
	while ( token != NULL)
	{
		nextToken = token->next;
		if ( token->hash == kPatternNoMatch && nextToken != NULL )
		{
			switch ( nextToken->hash )
			{
				/* The tokens we treat as suffixes */
			case kPatternCountryUK:
			case kPatternCountryUS:
			case kPatternCountryUSA:
			case kPatternYear:
				/* extend the kPatternNoMatch token to include the suffix */
				*(char *)token->end = ' ';
				token->end = nextToken->end;
				break;

			default:
				break;
			}
		}
		token = token->next;
	}
}

int parseName( string name )
{
    tToken * token = gTokenList.next;

    tokenizeName( name );

    mergeDigits();
    mergeNoMatch();

    debugf( 3, "%s\n", "after merging" );
    token = gTokenList.next;
    while ( token != NULL)
    {
        debugf( 3, "token: \'%s\', \'%s\' (%c)\n", lookupHash( token->hash ), token->start, token->seperator );

	    storeToken( token->hash, token->start );
	    token = token->next;
    }

    freeTokenList();

    return 0;
}


/*
 * carve up the path into directory path, basename and extension
 * then pass basename onto parseName() to be processed
 */
int parsePath( string path )
{
    int result = 0;

    addParam( gFileDict, kKeywordSource, path );

    string lastPeriod = strrchr( path, '.' );
    if ( lastPeriod != NULL )
    {
        addParam( gFileDict, kKeywordExtension, lastPeriod );
    }
    else
    {
        lastPeriod = path + strlen( path );
    }

    string lastSlash = strrchr( path, '/' );
    if ( lastSlash != NULL )
    {
        string p = strndup( path, lastSlash - path );
        addParam( gFileDict, kKeywordPath, p );
        free( (void *)p );

        ++lastSlash;
    }
    else
    {
        lastSlash = path; // no directories prefixed
    }

    string basename = strndup( lastSlash, lastPeriod - lastSlash );
    addParam( gFileDict, kKeywordBasename, basename );
    parseName( basename );
    free( (void *)basename );

    return result;
}

string buildString( string template )
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

                // scan the keyword and generate its hash
                hash = 0;

	            c = *t++;
	            while ( c != '\0' && c != '}' && c != '?' )
                {
                    if ( kKeywordMap[ c ] != kKeywordSeparator ) /* we ignore some characters when calculating the hash */
                    {
                        hash = fKeywordHashChar( hash, c );
                    }
                    c = *t++;
                }

                if ( hash != kKeywordTemplate ) // don't want to expand a {template} keyword in a template!
                {
                    string value = findParam( hash );

                    if ( value == NULL ) // not in the dictionaries, check for an environment variable
                    {
                        string envkey = strndup( k, t - k - 1 );
                        value = getenv( envkey );
                        // debugf( 3, "env=\"%s\", value=\"%s\"\n", envkey, value );
                        free( (void *)envkey );
                    }

                    if ( c != '?' )
                    {
                        // end of keyword, and not the beginning of a ternary expression
                        if ( value != NULL )
                        {
                            s = stpcpy( s, value );
                        }
                    }
                    else
                    {  // ternary operator, like {param?true:false} (true or false can be absent)

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
    int    result = 0;
    FILE * file;
    char   buffer[ 4096 ]; // 4K seems like plenty

    if ( eaccess( path, R_OK ) != 0 )   // only attempt to parse it if there's something accessible there
    {
	    // it's OK if the file is missing, otherwise complain
	    if ( errno != ENOENT )
	    {
		    fprintf( stderr,
		             "### Error: Unable to access config file \'%s\' (%d: %s)",
		             path, errno, strerror(errno));
		    result = errno;
	    }
    }
	else
	{
	    debugf( 3, "config file: \'%s\'\n", path );

	    file = fopen(path, "r");
        if (file == NULL)
        {
            fprintf( stderr, "### Error: Unable to open config file \'%s\' (%d: %s)\n",
                     path, errno, strerror(errno) );
            result = errno;
        }
        else
        {
            while ( fgets( buffer, sizeof( buffer ), file) != NULL )
            {
                trimTrailingWhitespace( buffer );
                debugf( 3,"line: \'%s\'\n", buffer );

                tHash hash = 0;
                string s = buffer;
                while (isspace(*s)) {
                    s++;
                }

                unsigned char c = (unsigned char) *s++;
                if (c != '\0') {
                    while (c != '\0' && c != '=') {
                        if ( c != kKeywordSeparator ) {
                            hash = fKeywordHashChar( hash, c );
                        }
                        c = (unsigned char) *s++;
                    }

                    if (c == '=') {
                        // skip over whitespace from the beginning of the value
                        while ( isspace(*s) ) {
                            s++;
                        }
	                    trimTrailingWhitespace( (char *)s );
                    }
                    debugf( 3,"hash = 0x%016lx, value = \'%s\'\n", hash, s);
                    addParam( dictionary, hash, s );
                }
            }
            fclose(file);
        }
    }

    return result;
}

/**
 * @brief Look for config files to process, and use them to update the main dictionary.
 *
 * First, look in /etc/<argv[0]>.conf then in ~/.config/<argv[0]>.conf, and finally the file
 * passed as a -c parameter, if any, then any parameters on the command line (except -c)
 * Where a parameter occurs more than once in a dictionary, the most recent definition 'wins'
 */

int parseConfig( string path )
{
    int result = 0;
    char temp[PATH_MAX];

    snprintf( temp, sizeof( temp ), "/etc/%s.conf", gMyName );
    debugf( 3, "/etc path: \"%s\"\n", temp );

    result = parseConfigFile( gMainDict, temp );

    if ( result == 0 )
    {
        string home = getenv("HOME");
        if ( home == NULL)
        {
            home = getpwuid( getuid() )->pw_dir;
        }
        if ( home != NULL )
        {
            snprintf( temp, sizeof( temp ), "%s/.config/%s.conf", home, gMyName );
            debugf( 3, "~ path: \"%s\"\n", temp );

            result = parseConfigFile( gMainDict, temp );
        }
    }

    if ( result == 0 && path != NULL )
    {
    	struct stat fileStat;

    	if ( stat( path, &fileStat ) != 0 )
	    {
		    fprintf( stderr, "### Error: config path '%s' is not valid (%d: %s)\n",
		    		 path, errno, strerror(errno) );
		    result = -1;
	    }
	    switch ( fileStat.st_mode & S_IFMT )
	    {
	    case S_IFDIR:
		    snprintf( temp, sizeof( temp ), "%s/%s.conf", path, gMyName );
		    break;

	    case S_IFLNK:
	    case S_IFREG:
		    strncpy( temp, path, sizeof( temp ) );
		    break;

	    default:
		    fprintf( stderr, "### Error: config path '%s' is neither a file nor directory.\n", path );
		    result = -1;
		    break;
	    }

    	if ( result == 0 )
	    {
		    debugf( 3, "-c path: %s\n", temp );
		    result = parseConfigFile( gMainDict, temp );
	    }
    }

    return result;
}

/**
 * @brief recurive function to walk the path looking for config files
 * @param gFileDict
 * @param path
 */
void _recurseConfig( tDictionary * dictionary, string path )
{
	char temp[PATH_MAX];

	if ( strlen(path) != 1 || (path[0] != '/' && path[0] != '.'))
	{
		strncpy( temp, path, sizeof( temp ) );
		_recurseConfig( dictionary, dirname( temp ) );
		/* check for a config file & if found, parse it */
		debugf( 3, "recurse = \'%s\'\n", path );
		snprintf( temp, sizeof(temp), "%s/%s.conf", path, gMyName );
		parseConfigFile( dictionary, temp );
	}
}

/**
   Traverse the path to the source file, looking for config files.
   Apply them in the reverse order, so ones lower in the hierarchy
   can override parameters defined in higher ones.
 */
int processConfigPath( string path )
{
	int  result = 0;
	char temp[PATH_MAX];
	char * absolute;

	/* dirname may modify its argument, so make a copy first */
	strncpy( temp, path, sizeof(temp) );
	absolute = realpath( dirname(temp), NULL );
	if ( absolute == NULL )
	{
		fprintf( stderr, "### Error: path \'%s\' appears to be invalid (%d: %s).\n",
				 path, errno, strerror(errno) );
		return -5;
	}
	else
	{
		debugf( 3, "abs = %s, cached = %s\n", absolute, gCachedPath );
		if ( gCachedPath == NULL || strcmp( gCachedPath, absolute ) != 0 )
		{
			debugf( 3, "absolute = \'%s\'\n", absolute );
			emptyDictionary( gPathDict );
			gCachedPath = absolute;
			_recurseConfig( gPathDict, absolute );
		}

		/* we may have picked up a new definition of {destination} as
		 * a result of parsing different config files. If so, we need
		 * to rebuild gSeriesDict to reflect the new destination */

		string destination = findParam( kKeywordDestination );

		if ( destination == NULL)
		{
			fprintf( stderr, "### Error: no destination defined.\n" );
			result = -3;
		}
		else
		{
			if ( gCachedSeries == NULL || strcmp( gCachedSeries, destination ) != 0 )
			{
				debugf( 3, "destination = \'%s\'\n", destination );
				// fill the dictionary with hashes of the directory names in the destination
				emptyDictionary( gSeriesDict );
				gCachedSeries = destination;
				buildSeriesDictionary( destination );
			}
		}
	}
	return result;
}

int processFile( string path )
{
    int result = 0;

    processConfigPath( path );

	parsePath( path );

    printDictionary( gFileDict );

    string template = findParam( kKeywordTemplate );

    if ( template == NULL)
    {
        fprintf( stderr, "### Error: no template found.\n" );
        result = -2;
    }
    else
    {
        debugf( 3, "template = \'%s\'\n", template );

        string output = buildString( template );
        string exec   = findParam( kKeywordExecute );
        if ( exec != NULL)
        {
	        result = system( output );
        }
        else
        {
	        printf( "%s\n", output );
        }
        free( (void *)output );
    }
    emptyDictionary( gFileDict );
    return result;
}

string usage =
"Command Line Options\n"
"  -d <string>  set {destination} parameter\n"
"  -t <string>  set {template} paameter\n"
"  -x           pass each output string to the shell to execute\n"
"  --           read from stdin\n"
"  -0           stdin is null-terminated (also implies '--' option)\n"
"  -v <level>   set the level of verbosity (debug info)\n";


int main( int argc, string argv[] )
{
    int  result;
    int  cnt;
    string configPath = NULL;
	time_t secsSinceEpoch;
	struct tm *timeStruct;

    gMainDict   = createDictionary( "Main" );
	gSeriesDict = createDictionary( "Series" );
	gPathDict   = createDictionary( "Path" );
	gFileDict   = createDictionary( "File" );

	gMyName = basename( strdup( argv[0] ) ); // posix flavor of basename modifies its argument

    secsSinceEpoch = time( NULL );
	timeStruct = localtime( &secsSinceEpoch );
	if ( timeStruct != NULL )
	{
		gNextYear = timeStruct->tm_year + 1900 + 1;
	}

    int k = 1;
	cnt = argc;
    for ( int i = 1; i < argc; i++ )
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

    result = parseConfig( configPath );

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
                fprintf( stderr, "%s", usage );
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
                    addParam( gMainDict, kKeywordDestination, argv[ i ] );
                    --cnt;
                    ++i;
                    break;

                case 't':   // template
                    addParam( gMainDict, kKeywordTemplate, argv[ i ] );
                    --cnt;
                    ++i;
                    break;

                case 'x':   // execute
                    addParam( gMainDict, kKeywordExecute, "yes" );
                    break;

                case '-':   // also read lines from stdin
                    addParam( gMainDict, kKeywordStdin, "yes" );
                    break;

                case '0':   // entries from stdio are terminated with NULLs
                    addParam( gMainDict, kKeywordStdin, "yes" );
                    addParam( gMainDict, kKeywordNullTermination, "yes" );
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
                    fprintf( stderr, "%s", usage );
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

    for ( int i = 1; i < argc; i++ )
    {
	    debugf( 3, "b: i = %d, k = %d, cnt = %d, \'%s\'\n", i, k, cnt, argv[i] );

	    // is it an option?
	    if ( argv[i][0] == '-' )
	    {
		    char option = argv[i][1];
		    if ( argv[i][2] != '\0' )
		    {
			    fprintf( stderr, "### Error: option \'%s\' not understood.\n", argv[i] );
			    result = -1;
		    }
		    else
		    {
			    --cnt;

			    switch ( option )
			    {
			    case 'd':   // destination
				    addParam( gMainDict, kKeywordDestination, argv[i] );
				    --cnt;
				    ++i;
				    break;

			    case 't':   // template
				    addParam( gMainDict, kKeywordTemplate, argv[i] );
				    --cnt;
				    ++i;
				    break;

			    case 'x':   // execute
				    addParam( gMainDict, kKeywordExecute, "yes" );
				    break;

			    case '-':   // also read lines from stdin
				    addParam( gMainDict, kKeywordStdin, "yes" );
				    break;

			    case '0':   // entries from stdio are terminated with NULLs
				    addParam( gMainDict, kKeywordNullTermination, "yes" );
				    break;

			    case 'v': //verbose output, i.e. debug logging
				    if ( i < argc - 1 )
				    {
					    ++i;
					    --cnt;

					    gDebugLevel = atoi( argv[i] );
					    fprintf( stderr, "verbosity = %d\n", gDebugLevel );
				    }
				    break;

			    default:
				    ++cnt;
				    --i; // point back at the original option
				    fprintf( stderr, "### Error: option \'%s\' not understood.\n", argv[i] );
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

    printDictionary( gMainDict );

    for ( int i = 1; i < argc && result == 0; ++i )
    {
        debugf( 3, "%d: \'%s\'\n", i, argv[ i ] );
        processFile( argv[i] );
    }

    // should we also read from stdin?
    if ( findParam( kKeywordStdin ) != NULL )
    {
        char line[PATH_MAX];

        if ( findParam( kKeywordNullTermination ) != NULL )
        {
            // ...therefore lines are terminated by \0
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
                    processFile( line );

                    p = line;
                    cnt = sizeof( line );
                }
            }
        }
        else
        {
            while (!feof(stdin))
            {
                // ...otherwise lines are terminated by \n
                fgets( line, sizeof(line), stdin );

                // lop off the inevitable trailing newline(s)/whitespace
                trimTrailingWhitespace( line );
                debugf( 3,"eol: %s\n", line);
                processFile( line);
            }
        }
    }

    // all done, clean up.
	destroyDictionary( gFileDict );
	destroyDictionary( gPathDict );
	destroyDictionary( gSeriesDict );
	destroyDictionary( gMainDict );

    return result;
}
