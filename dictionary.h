//
// Created by root on 8/22/19.
//

#ifndef CHANDVR2PLEX_DICTIONARY_H
#define CHANDVR2PLEX_DICTIONARY_H

typedef unsigned long tHash;

typedef struct tParam {
    struct tParam * next;
    tHash           hash;
    char *          value;
} tParam;

typedef struct {
    tParam * head;
    const char * name;
} tDictionary;

tDictionary * createDictionary( const char * name );
void destroyDictionary( tDictionary * dictionary );
void printDictionary( tDictionary * dictionary );
int addParam( tDictionary * dictionary, tHash hash, char * value );
char * findValue( tDictionary * dictionary, tHash hash );

#endif //CHANDVR2PLEX_DICTIONARY_H
