#include "BlockCache.h"
#include "DataSource.h"
#include "FilesystemException.h"

#include <algorithm>
#include <cerrno>
#include <memory>
#include <sys/stat.h>

using std::move;
using std::unique_ptr;

namespace RT11FS {

BlockCache::BlockCache(DataSource *dataSource) 
  : dataSource(dataSource)
{
  struct stat st;

  auto err = dataSource->stat(&st);
  if (err) {
    throw FilesystemException {err, "Could not stat disk image"};
  }

  sectors = st.st_size / Block::SECTOR_SIZE;
}

BlockCache::~BlockCache()
{
}

// Retrieve a block, reading if needed. A block request may not overlap with any other 
// existing block.
auto BlockCache::getBlock(int sector, int count) -> Block *
{
  auto cacheIter = begin(blocks);

  for (; cacheIter != end(blocks); cacheIter++) {
    auto bp = cacheIter->get();

    if (bp->getSector() == sector) {
      if (bp->getCount() != count) {
        throw FilesystemException {-EINVAL, "Asking for wrong number of sectors in block cache"};
      }
      bp->addRef();
      return bp;
    }

    if (sector >= bp->getSector() + bp->getCount()) {
      continue;
    }

    if (sector + count <= bp->getSector()) {
      break;
    }

    if (bp->getCount() != count) {
      throw FilesystemException {-EINVAL, "Block cache request would overlap existing block"};
    }
  }

  auto bp = new Block {sector, count};
  bp->read(dataSource);

  blocks.insert(cacheIter, move(unique_ptr<Block>{bp}));
  return bp;
}


// Release ownership of a block
auto BlockCache::putBlock(Block *bp) -> void
{  
  bp->release();
}


// Resize a block to a new number of sectors. If the block grows, it cannot be made to
// overlap succeeding blocks in the block list. A block may not be downsized to zero or
// a negative sector count.
auto BlockCache::resizeBlock(Block *bp, int count) -> void
{
  if (count <= 0) {
    throw FilesystemException {-EINVAL, "Block resize to non-positive size"};    
  }

  auto cacheIter = find_if(begin(blocks), end(blocks), [bp](const auto &bup) { return bup.get() == bp; });
  if (cacheIter == end(blocks)) {
    throw FilesystemException {-EINVAL, "Block cache ask to resize nonexistent block"};
  }

  cacheIter++;

  if (cacheIter != end(blocks) && bp->getSector() + count > (*cacheIter)->getSector()) {
    throw FilesystemException {-EINVAL, "Block resize would cause overlap, or non-positive size"};    
  }

  bp->resize(count, dataSource);
}

}
