//
// Created by Paul on 4/4/2019.
//

#ifndef CHANDVR2PLEX_H
#define CHANDVR2PLEX_H

#if CMAKE_BUILD_TYPE == Debug
#define DEBUG 1
#endif

typedef const char * string;

extern int gDebugLevel;
#define debugf( level, format, ... ) do { if (gDebugLevel >= level) fprintf( stderr, format, __VA_ARGS__ ); } while (0)

/* patterns in the source */
#define kPattern_noMatch    0x0000000000000001  // no pattern matched
#define kPattern_SnnEnn     0x00000003f9b381c4  // SnnEnn
#define kPattern_SyyyyEnn   0x00001ccd9c944b04  // SyyyyEnn
#define kPattern_SnnEn      0x00000000176a4622  // SnnEn
#define kPattern_SnEnn      0x00000000176e12d4  // SnEnn
#define kPattern_SnEn       0x00000000008e24fa  // SnEn
#define kPattern_Ennn       0x00000000007d8ad8  // Ennn
#define kPattern_Ennnn      0x00000000156bd8a0  // Ennnn
#define kPattern_nXnn       0x0000000000410fb0  // nXnn
#define kPattern_nnXnn      0x0000000009e517b0  // nnXnn
#define kPattern_Date       0x0055006b8132df24  // yyyy-mm-dd
#define kPattern_DateTime   0x20ef704b1d973420  // yyyy-mm-dd-hhmm
#define kPattern_TwoDigits  0x0000000000000870  // nn
#define kPattern_FourDigits 0x00000000003ad770  // nnnn
#define kPattern_SixDigits  0x00000c25ae664770  // nnnnnn

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


#endif // CHANDVR2PLEX_H
