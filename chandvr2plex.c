//
// Created by Paul on 4/4/2019.
//

#include "chandvr2plex.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef enum
{
    kNull = 0,
    kControl,
    kSpace,
    kNumber,
    kPunctuation,
    kAlphabetic,
    kSeason,
    kEpisode,
    kDash,
    kPeriod,
    kSeparator,
} tCharClass;

tCharClass classifyChar[256] = {
    kNull,          // 0x00
    kControl,       // 0x01
    kControl,       // 0x02
    kControl,       // 0x03
    kControl,       // 0x04
    kControl,       // 0x05
    kControl,       // 0x06
    kControl,       // 0x07
    kControl,       // 0x08
    kControl,       // 0x09
    kControl,       // 0x0a
    kControl,       // 0x0b
    kControl,       // 0x0c
    kControl,       // 0x0d
    kControl,       // 0x0e
    kControl,       // 0x0f
    kControl,       // 0x10
    kControl,       // 0x11
    kControl,       // 0x12
    kControl,       // 0x13
    kControl,       // 0x14
    kControl,       // 0x15
    kControl,       // 0x16
    kControl,       // 0x17
    kControl,       // 0x18
    kControl,       // 0x19
    kControl,       // 0x1a
    kControl,       // 0x1b
    kControl,       // 0x1c
    kControl,       // 0x1d
    kControl,       // 0x1e
    kControl,       // 0x1f
    kSpace,         // ' '
    kPunctuation,   // '!'
    kPunctuation,   // '"'
    kPunctuation,   // '#'
    kPunctuation,   // '$'
    kPunctuation,   // '%'
    kPunctuation,   // '&'
    kPunctuation,   // '''
    kPunctuation,   // '('
    kPunctuation,   // ')'
    kPunctuation,   // '*'
    kPunctuation,   // '+'
    kPunctuation,   // ','
    kDash,          // '-'
    kPeriod,        // '.'
    kPunctuation,   // '/'
    kNumber,        // '0'
    kNumber,        // '1'
    kNumber,        // '2'
    kNumber,        // '3'
    kNumber,        // '4'
    kNumber,        // '5'
    kNumber,        // '6'
    kNumber,        // '7'
    kNumber,        // '8'
    kNumber,        // '9'
    kPunctuation,   // ':'
    kPunctuation,   // ';'
    kPunctuation,   // '<'
    kPunctuation,   // '='
    kPunctuation,   // '>'
    kPunctuation,   // '?'
    kPunctuation,   // '@'
    kAlphabetic,    // 'A'
    kAlphabetic,    // 'B'
    kAlphabetic,    // 'C'
    kAlphabetic,    // 'D'
    kEpisode,       // 'E'
    kAlphabetic,    // 'F'
    kAlphabetic,    // 'G'
    kAlphabetic,    // 'H'
    kAlphabetic,    // 'I'
    kAlphabetic,    // 'J'
    kAlphabetic,    // 'K'
    kAlphabetic,    // 'L'
    kAlphabetic,    // 'M'
    kAlphabetic,    // 'N'
    kAlphabetic,    // 'O'
    kAlphabetic,    // 'P'
    kAlphabetic,    // 'Q'
    kAlphabetic,    // 'R'
    kSeason,        // 'S'
    kAlphabetic,    // 'T'
    kAlphabetic,    // 'U'
    kAlphabetic,    // 'V'
    kAlphabetic,    // 'W'
    kSeparator,     // 'X'
    kAlphabetic,    // 'Y'
    kAlphabetic,    // 'Z'
    kPunctuation,   // '['
    kPunctuation,   // '\'
    kPunctuation,   // ']'
    kPunctuation,   // '^'
    kPunctuation,   // '_'
    kPunctuation,   // '`'
    kAlphabetic,    // 'a'
    kAlphabetic,    // 'b'
    kAlphabetic,    // 'c'
    kAlphabetic,    // 'd'
    kEpisode,       // 'e'
    kAlphabetic,    // 'f'
    kAlphabetic,    // 'g'
    kAlphabetic,    // 'h'
    kAlphabetic,    // 'i'
    kAlphabetic,    // 'j'
    kAlphabetic,    // 'k'
    kAlphabetic,    // 'l'
    kAlphabetic,    // 'm'
    kAlphabetic,    // 'n'
    kAlphabetic,    // 'o'
    kAlphabetic,    // 'p'
    kAlphabetic,    // 'q'
    kAlphabetic,    // 'r'
    kSeason,        // 's'
    kAlphabetic,    // 't'
    kAlphabetic,    // 'u'
    kAlphabetic,    // 'v'
    kAlphabetic,    // 'w'
    kSeparator,     // 'x'
    kAlphabetic,    // 'y'
    kAlphabetic,    // 'z'
    kPunctuation,   // '{'
    kPunctuation,   // '|'
    kPunctuation,   // '}'
    kPunctuation,   // '~'
    kNull,          // 0x7f
    kNull,          // 0x80
    kNull,          // 0x81
    kNull,          // 0x82
    kNull,          // 0x83
    kNull,          // 0x84
    kNull,          // 0x85
    kNull,          // 0x86
    kNull,          // 0x87
    kNull,          // 0x88
    kNull,          // 0x89
    kNull,          // 0x8a
    kNull,          // 0x8b
    kNull,          // 0x8c
    kNull,          // 0x8d
    kNull,          // 0x8e
    kNull,          // 0x8f
    kNull,          // 0x90
    kNull,          // 0x91
    kNull,          // 0x92
    kNull,          // 0x93
    kNull,          // 0x94
    kNull,          // 0x95
    kNull,          // 0x96
    kNull,          // 0x97
    kNull,          // 0x98
    kNull,          // 0x99
    kNull,          // 0x9a
    kNull,          // 0x9b
    kNull,          // 0x9c
    kNull,          // 0x9d
    kNull,          // 0x9e
    kNull,          // 0x9f
    kNull,          // 0xa0
    kNull,          // 0xa1
    kNull,          // 0xa2
    kNull,          // 0xa3
    kNull,          // 0xa4
    kNull,          // 0xa5
    kNull,          // 0xa6
    kNull,          // 0xa7
    kNull,          // 0xa8
    kNull,          // 0xa9
    kNull,          // 0xaa
    kNull,          // 0xab
    kNull,          // 0xac
    kNull,          // 0xad
    kNull,          // 0xae
    kNull,          // 0xaf
    kNull,          // 0xb0
    kNull,          // 0xb1
    kNull,          // 0xb2
    kNull,          // 0xb3
    kNull,          // 0xb4
    kNull,          // 0xb5
    kNull,          // 0xb6
    kNull,          // 0xb7
    kNull,          // 0xb8
    kNull,          // 0xb9
    kNull,          // 0xba
    kNull,          // 0xbb
    kNull,          // 0xbc
    kNull,          // 0xbd
    kNull,          // 0xbe
    kNull,          // 0xbf
    kNull,          // 0xc0
    kNull,          // 0xc1
    kNull,          // 0xc2
    kNull,          // 0xc3
    kNull,          // 0xc4
    kNull,          // 0xc5
    kNull,          // 0xc6
    kNull,          // 0xc7
    kNull,          // 0xc8
    kNull,          // 0xc9
    kNull,          // 0xca
    kNull,          // 0xcb
    kNull,          // 0xcc
    kNull,          // 0xcd
    kNull,          // 0xce
    kNull,          // 0xcf
    kNull,          // 0xd0
    kNull,          // 0xd1
    kNull,          // 0xd2
    kNull,          // 0xd3
    kNull,          // 0xd4
    kNull,          // 0xd5
    kNull,          // 0xd6
    kNull,          // 0xd7
    kNull,          // 0xd8
    kNull,          // 0xd9
    kNull,          // 0xda
    kNull,          // 0xdb
    kNull,          // 0xdc
    kNull,          // 0xdd
    kNull,          // 0xde
    kNull,          // 0xdf
    kNull,          // 0xe0
    kNull,          // 0xe1
    kNull,          // 0xe2
    kNull,          // 0xe3
    kNull,          // 0xe4
    kNull,          // 0xe5
    kNull,          // 0xe6
    kNull,          // 0xe7
    kNull,          // 0xe8
    kNull,          // 0xe9
    kNull,          // 0xea
    kNull,          // 0xeb
    kNull,          // 0xec
    kNull,          // 0xed
    kNull,          // 0xee
    kNull,          // 0xef
    kNull,          // 0xf0
    kNull,          // 0xf1
    kNull,          // 0xf2
    kNull,          // 0xf3
    kNull,          // 0xf4
    kNull,          // 0xf5
    kNull,          // 0xf6
    kNull,          // 0xf7
    kNull,          // 0xf8
    kNull,          // 0xf9
    kNull,          // 0xfa
    kNull,          // 0xfb
    kNull,          // 0xfc
    kNull,          // 0xfd
    kNull,          // 0xfe
    kNull           // 0xff
};

int main( int argc, char * argv[] )
{
    exit( 0 );
}