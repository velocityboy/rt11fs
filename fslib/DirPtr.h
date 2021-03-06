// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __DIRPTR_H_
#define __DIRPTR_H_

#include <cstdint>

namespace RT11FS {
class Block;

/**
 * Represents a pointer to a directory entry, by segment and index.
 *
 * Understands how to traverse the directory in either direction.
 */
class DirPtr
{
public:
  DirPtr();
  DirPtr(Block *dirblk);

  auto beforeStart() const { return segment == -1; }
  auto afterEnd() const { return segment == 0; }
  auto offset(int delta = 0) const -> int;
  auto setSegment(int seg) -> void;
  auto setIndex(int idx) { index = idx; }
  auto getSegment() const { return segment; }
  auto getIndex() const { return index; }
  auto incIndex() { index++; }
  auto getDataSector() const { return datasec; }
  auto getWord(int offs) const -> uint16_t;
  auto getByte(int offs) const -> uint8_t;
  auto setByte(int offs, uint8_t v) -> void;
  auto setWord(int offs, uint16_t v) -> void;
  auto setSegmentWord(int offset, uint16_t v) -> void;
  auto getSegmentWord(int offset) -> uint16_t;
  auto hasStatus(uint16_t mask) const -> bool;

  auto operator++() -> DirPtr &;
  auto operator++(int) -> DirPtr;
  auto next() const -> DirPtr;

  auto operator--() -> DirPtr &;
  auto operator--(int) -> DirPtr;
  auto prev() const -> DirPtr;

  /**
   * Check if the pointer points to a valid entry
   */
  operator bool() const { return !beforeStart() && !afterEnd(); }

private:
  Block *dirblk;      /*!< Pointer to a block that contains the entire directory */
  int entrySize;      /*!< The size of a directory entry, including any extra bytes */
  int segment;        /*!< The one-based index of the segment containing the pointed to entry */
  int index;          /*!< The zero-based index of the entry within its containing segment */
  int segbase;        /*!< The offset of the current segment in the directory block */
  int datasec;        /*!< The first data block of the referenced file */

  auto increment() -> void;
  auto decrement() -> void;
};
}


#endif
