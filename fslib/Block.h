// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __BLOCK_H_
#define __BLOCK_H_

#include <cstdint>
#include <vector>
#include <cstring>

namespace RT11FS {
class DataSource;


/**
 * A block of data from the disk.
 *
 * A block may contain one more more sectors. A block tracks if it has been changed
 * (the dirty flag). A block should only be changed by one of the mutators in order
 * for the dirty flag to be maintained.
 */
class Block
{
public:
  static const int SECTOR_SIZE = 512;

  Block(int sector, int count);

  auto getByte(int offset) -> uint8_t;
  auto extractWord(int offset) -> uint16_t;

  auto copyOut(int offset, int bytes, char *dest) -> void;
  auto copyIn(int offset, int bytes, const char *src) -> void;
  auto copyWithinBlock(int sourceOffset, int destOffset, int count) -> void;
  auto copyFromOtherBlock(Block *source, int sourceOffset, int destOffset, int count) -> void;
  auto zeroFill(int offset, int count) -> void;

  auto setByte(int offset, uint8_t value) -> void;
  auto setWord(int offset, uint16_t value) -> void;

  /**
   * @return the starting sector of the block.
   */
  auto getSector() { return sector; }

  /**
   * @return the number of sectors contained in the block.
   */
  auto getCount() { return count; }

  auto read(DataSource *dataSource) -> void;
  auto write(DataSource *dataSource) -> void;
  auto resize(int newCount, DataSource *dataSource) -> void;

  auto addRef() { return ++refcount; }
  auto release() { return --refcount; }

  /**
   * @return true if the block needs to be written back to disk.
   */
  auto isDirty() const { return dirty; }

private:
  int sector;
  int count;
  int refcount;
  bool dirty;
  std::vector<uint8_t> data;
};
}

#endif
