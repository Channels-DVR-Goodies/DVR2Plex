//
// Created by root on 8/22/19.
//
#include "chandvr2plex.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dictionary.h"

tDictionary * createDictionary( const char * name )
{
    tDictionary * result = (tDictionary *)calloc( 1, sizeof(tDictionary) );
    result->name = name;
    return result;
}

void emptyDictionary( tDictionary * dictionary )
{
	tParam * p = dictionary->head;
	dictionary->head = NULL;

	while ( p != NULL)
	{
		if ( p->value != NULL )
		{
			free( (void *) p->value );
		}

		tParam * next = p->next;
		free( p );
		p = next;
	}
}

void destroyDictionary( tDictionary * dictionary )
{
    emptyDictionary( dictionary );
    free( dictionary );
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
