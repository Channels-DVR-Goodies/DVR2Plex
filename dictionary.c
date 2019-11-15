//
// Created by root on 8/22/19.
//
#include "chandvr2plex.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dictionary.h"

typedef struct {
    tHash   key;
    string  label;
} tPrintHashMapping;

tPrintHashMapping printHash[] = {
        { kPattern_noMatch,     "noMatch"},     // not a pattern we recognize
        { kPattern_SnnEnn,      "SnnEnn" },     // SnnEnn
        { kPattern_SyyyyEnn,    "SyyyyEnn" },   // SyyyyEnn
        { kPattern_SnnEn,       "SnnEn" },      // SnnEn
        { kPattern_SnEnn,       "SnEnn" },      // SnEnn
        { kPattern_SnEn,        "SnEn" },       // SnEn
        { kPattern_Ennn,        "Ennn" },       // Ennn
        { kPattern_Ennnn,       "Ennnn" },      // Ennnn
        { kPattern_nXnn,        "nXnn" },       // nXnn
        { kPattern_nnXnn,       "nnXnn" },      // nnXnn
        { kPattern_Date,        "Date" },       // yyyy-mm-dd
        { kPattern_DateTime,    "DateTime" },   // yyyy-mm-dd-hhmm
        { kPattern_TwoDigits,   "two digits"},  // nn
        { kPattern_FourDigits,  "four digits"}, // nnnn
        { kPattern_SixDigits,   "six digits"},  // nnnnnn

        { kKeyBasename,         "{Basename}"},
        { kKeyDateRecorded,     "{DateRecorded}"},
        { kKeyDestination,      "{Destination}"},
        { kKeyDestSeries,       "{DestSeries}"},
        { kKeyEpisode,          "{Episode}"},
        { kKeyExtension,        "{Extension}"},
        { kKeyFirstAired,       "{FirstAired}"},
        { kKeyPath,             "{Path}"},
        { kKeySeason,           "{Season}"},
        { kKeySeasonFolder,     "{SeasonFolder}"},
        { kKeySeries,           "{Series}"},
        { kKeySource,           "{Source}"},
        { kKeyTemplate,         "{Template}"},
        { kKeyTitle,            "{Title}"},
        { kKeyExecute,          "{Execute}"},
        { kKeyStdin,            "{Stdin}"},
        { kKeyNullTermination,  "{NullTermination}"},

        { 0, NULL }
};


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

        // debugf( 3, "{%s}\n", p->value );
        next = p->next;
        free( (void *)p->value );
        free( p );
        p = next;
    }
}

string lookupHash( tHash hash )
{
    tPrintHashMapping * map = printHash;
    while (map->key != 0)
    {
        if ( hash == map->key )
        {
            return map->label;
        }
        map++;
    }
    return "<unknown>";
}

void printDictionary( tDictionary * dictionary )
{
    if ( dictionary != NULL )
    {
        debugf( 3, "...%s dictionary...\n", dictionary->name);

        tParam * p = dictionary->head;
        while ( p != NULL )
        {
            debugf( 3, "%16s: \"%s\"\n", lookupHash(p->hash), p->value );
            p = p->next;
        }
    }
}

int addParam( tDictionary * dictionary, tHash hash, const char * value )
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

string findValue( tDictionary * dictionary, tHash hash )
{
    string result = NULL;
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
