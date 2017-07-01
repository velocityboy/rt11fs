#ifndef __BLOCK_CACHE_H_
#define __BLOCK_CACHE_H_

#include "Block.h"

#include <list>
#include <memory>

namespace RT11FS {
class BlockCache {
public:
  BlockCache(int fd);
  ~BlockCache();

  auto getBlock(int sector, int count) -> Block *;
  auto putBlock(Block *bp) -> void;
  auto resizeBlock(Block *bp, int count) -> void;
  auto getVolumeSectors() { return sectors; }

private:
  int fd;
  int sectors;
  std::list<std::unique_ptr<Block>> blocks;
};
}

#endif
