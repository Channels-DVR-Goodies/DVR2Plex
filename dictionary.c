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
        free( p->value );
        free( p );
        p = next;
    }
}

void printDictionary( tDictionary * dictionary )
{
    if ( dictionary != NULL )
    {
        debugf( 3, "...%s dictionary...\n", dictionary->name);

        tParam * p = dictionary->head;
        while ( p != NULL )
        {
            debugf( 3, "0x%016lx, \"%s\"\n", p->hash, p->value );
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
