#ifndef __BLOCK_H_
#define __BLOCK_H_

#include <cstdint>
#include <vector>

namespace RT11FS {
class Block
{
public:
  static const int SECTOR_SIZE = 512;

  Block(int sector, int count);

  auto extractWord(int offset) -> uint16_t;

  auto copyOut(int offset, int bytes, char *dest) -> void;
  auto copyWithinBlock(int sourceOffset, int destOffset, int count) -> void;
  auto copyFromOtherBlock(Block *source, int sourceOffset, int destOffset, int count) -> void;

  auto setByte(int offset, uint8_t value) -> void;
  auto setWord(int offset, uint16_t value) -> void;

  auto getSector() { return sector; }
  auto getCount() { return count; }

  auto read(int fd) -> void;
  auto resize(int newCount, int fd) -> void;

  auto addRef() { return ++refcount; }
  auto release() { return --refcount; }

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
