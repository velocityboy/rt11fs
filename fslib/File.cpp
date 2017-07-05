#include "Block.h"
#include "BlockCache.h"
#include "File.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using std::cerr;
using std::endl;
using std::min;

namespace RT11FS {

File::File(BlockCache *cache, Directory *dir, const DirEnt &dirent)
  : cache(cache)
  , dir(dir)
  , dirent(dirent)
{  
}

auto File::read(char *buffer, size_t count, off_t offset) -> int
{
  auto end = offset + count;
  auto got = int {0};

  while (offset < end) {
    auto sector = offset / Block::SECTOR_SIZE;
    auto secoffs = offset % Block::SECTOR_SIZE;
    auto blk = cache->getBlock(dirent.sector0 + sector, 1);

    size_t leftInRead = end - offset;
    size_t leftInBlock = Block::SECTOR_SIZE - secoffs;
    auto tocopy = min(leftInBlock, leftInRead);

    blk->copyOut(secoffs, tocopy, buffer);

    cache->putBlock(blk);

    buffer += tocopy;
    got += tocopy;
    offset += tocopy;
  }

  return got;
}

auto File::write(const char *buffer, size_t count, off_t offset) -> int
{
  auto end = offset + count;
  auto got = int {0};
  auto extendFile = end > dirent.length;

  if (extendFile) {
    auto dirp = dir->getDirPointer(dirent.rad50_name);
    if (dirp.afterEnd()) {
      return -ENOENT;
    }

    auto err = dir->truncate(dirp, end);
    if (err < 0) {
      return err;
    }

    // truncate may have moved the file
    auto success = dir->getEnt(dirp, dirent);
    assert(success);

  }

  while (offset < end) {
    auto sector = offset / Block::SECTOR_SIZE;
    auto secoffs = offset % Block::SECTOR_SIZE;
    auto blk = cache->getBlock(dirent.sector0 + sector, 1);

    size_t leftInRead = end - offset;
    size_t leftInBlock = Block::SECTOR_SIZE - secoffs;
    auto tocopy = min(leftInBlock, leftInRead);

    blk->copyIn(secoffs, tocopy, buffer);

    if (extendFile && secoffs + tocopy < Block::SECTOR_SIZE) {
      // if we're extending the file and this is the last sector, it may have
      // garbage past the end if the file was moved
      blk->zeroFill(secoffs + tocopy, Block::SECTOR_SIZE - (secoffs + tocopy));
    }

    cache->putBlock(blk);

    buffer += tocopy;
    got += tocopy;
    offset += tocopy;
  }

  return got;
}

auto File::truncate(off_t size) -> int
{
  auto dirp = dir->getDirPointer(dirent.rad50_name);
  if (dirp.afterEnd()) {
    return -ENOENT;
  }

  auto err = dir->truncate(dirp, size);
  if (err < 0) {
    return err;
  }

  dir->getEnt(dirp, dirent);  

  return 0;
}


}
