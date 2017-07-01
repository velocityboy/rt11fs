#ifndef __DIRCONST_H_
#define __DIRCONST_H_

#include <array>
#include <cstdint>

namespace RT11FS {
namespace Dir {
const uint16_t E_TENT = 0000400;        // entry is tentative (open)
const uint16_t E_MPTY = 0001000;        // entry is free space
const uint16_t E_PERM = 0002000;        // entry is permanent (a real file)
const uint16_t E_EOS  = 0004000;        // entry marks end of segment
const uint16_t E_READ = 0040000;        // entry is read-only file
const uint16_t E_PROT = 0100000;        // entry is protected
const uint16_t E_PRE  = 0000020;        // entry has prefix blocks

// segment header on disk 
const uint16_t TOTAL_SEGMENTS = 0;        // total segments allocated for directory
const uint16_t NEXT_SEGMENT = 2;          // 1-based index of next segment
const uint16_t HIGHEST_SEGMENT = 4;       // highest segment in use (only maintained in segment 1)
const uint16_t EXTRA_BYTES = 6;           // extra bytes at the end of each dir entry
const uint16_t SEGMENT_DATA_BLOCK = 8;    // first disk block of first file in segment
const uint16_t FIRST_ENTRY_OFFSET = 10;   // offset of first entry in segment

// directory entry on disk 
const uint16_t STATUS_WORD = 0;           // offset of status word in entry
const uint16_t FILENAME_WORDS = 2;        // offset of filename (3x rad50 words)
const uint16_t TOTAL_LENGTH_WORD = 8;     // offset of file length (in blocks)
const uint16_t JOB_BYTE = 10;             // if open (E_TENT), job 
const uint16_t CHANNEL_BYTE = 11;         // if open (E_TENT), channel
const uint16_t CREATION_DATE_WORD = 12;   // creation date (packed word)
const uint16_t ENTRY_LENGTH = 14;         // length of entry with no extra bytes

const uint16_t FIRST_SEGMENT_SECTOR = 6;  // sector address of first sector of seg #1
const uint16_t SECTORS_PER_SEGMENT = 2;

const uint16_t FILENAME_LENGTH = 3;       /*!< number of rad50 words in a filename */
using Rad50Name = std::array<uint16_t, FILENAME_LENGTH>;

inline auto operator ==(const Rad50Name &left, const Rad50Name &right) 
{
  return 
    left.at(0) == right.at(0) && 
    left.at(1) == right.at(1) &&
    left.at(2) == right.at(2);
}

}}

#endif
