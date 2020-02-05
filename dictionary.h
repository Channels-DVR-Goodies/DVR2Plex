//
// Created by root on 8/22/19.
//

#ifndef DVR2PLEX_DICTIONARY_H
#define DVR2PLEX_DICTIONARY_H

typedef unsigned long tHash;

typedef struct tParam {
    struct tParam * next;
    const char    * value;
    tHash           hash;
} tParam;

typedef struct {
    tParam     * head;
    const char * name;
} tDictionary;

tDictionary *  createDictionary( const char * name );
         void  emptyDictionary( tDictionary * dictionary );
         void  destroyDictionary( tDictionary * dictionary );
       string  lookupHash( tHash );
         void  printDictionary( tDictionary * dictionary);
          int  addParam( tDictionary * dictionary, tHash hash, string value );
       string  findValue( tDictionary * dictionary, tHash hash );

#endif // DVR2PLEX_DICTIONARY_H
