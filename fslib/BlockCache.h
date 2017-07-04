#ifndef __BLOCK_CACHE_H_
#define __BLOCK_CACHE_H_

#include "Block.h"

#include <list>
#include <memory>

namespace RT11FS {
class DataSource;

class BlockCache {
public:
  BlockCache(DataSource *dataSource);
  ~BlockCache();

  auto getBlock(int sector, int count) -> Block *;
  auto putBlock(Block *bp) -> void;
  auto resizeBlock(Block *bp, int count) -> void;
  auto getVolumeSectors() { return sectors; }
  auto sync() -> void;

private:
  DataSource *dataSource;
  int sectors;
  std::list<std::unique_ptr<Block>> blocks;
};
}

#endif
