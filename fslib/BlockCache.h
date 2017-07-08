// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __BLOCK_CACHE_H_
#define __BLOCK_CACHE_H_

#include "Block.h"

#include <list>
#include <memory>

namespace RT11FS {
class DataSource;

/**
 * The cache maintains blocks in memory representing data from an underlying
 * data source. It manages multiple clients wanting access to the data,
 * and writing the data back to the data source as needed.
 *
 * A block represents one or more sectors on the underlying media.
 *
 * It is an error for any two blocks in the cache to overlap, as this would
 * cause the same data on disk to be represented in two different blocks.
 *
 */
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
