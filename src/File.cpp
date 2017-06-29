#include "Block.h"
#include "BlockCache.h"
#include "File.h"

#include <iostream>

using std::cerr;
using std::endl;

namespace RT11FS {

File::File(BlockCache *cache, const DirEnt &dirent)
  : cache(cache)
  , dirent(dirent)
{  
}

auto File::truncate(off_t size) -> int
{
  return -ENOSYS;
}

auto File::read(char *buffer, size_t count, off_t offset) -> int
{
  auto end = offset + count;
  auto got = int {0};

  while (offset < end) {
    auto sector = offset / Block::SECTOR_SIZE;
    auto secoffs = offset % Block::SECTOR_SIZE;
    auto blk = cache->getBlock(dirent.sector0 + sector, 1);
    auto tocopy = Block::SECTOR_SIZE - secoffs;

    blk->copyOut(secoffs, tocopy, buffer);

    cache->putBlock(blk);

    buffer += tocopy;
    got += tocopy;
    offset += tocopy;
  }

  return got;
}

}
