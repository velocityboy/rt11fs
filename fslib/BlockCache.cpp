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

/**
 * Construct a block cache.
 *
 * @param dataSource the data source containing the physical data
 * storage for the blocks.
 */
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

/**
 * Retrieve a block.
 *
 * If the block is already in the cache, a pointer to it will be returned; else
 * the data will be read into a new block.
 *
 * Blocks are reference counted; a block will never be evicted from the cache if
 * it had a non-zero reference count. Each call to `getBlock' must eventually be
 * balanced by a call to `putBlock' when the caller is done with the block.
 *
 * An exception will be raised if the requested block would overlap other block(s)
 * in the cache, or if there is an I/O error filling the block.
 *
 * @param sector the starting sector of the requested block.
 * @param count the number of sectors to return/
 * @return a pointer to a block containing the requested data
 */
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

/**
 * Release ownership of a block.
 */
auto BlockCache::putBlock(Block *bp) -> void
{  
  bp->release();
}

/** 
 * Resize a block to a new number of sectors. 
 * 
 * If the block grows, it cannot be made to overlap succeeding blocks in the 
 * cache. A block may not be downsized to zero or a negative sector count.
 * If either of these are requested, an exception will be thrown.
 *
 * If the block is grown, then data will be read to fill the new space. If an 
 * I/O error occurs while this is being done, an exception will thrown.
 *
 * @param bp the block to resize.
 * @param count the new size of the block.
 */
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

/**
 * Write all dirty blocks to disk.
 *
 * Will throw on I/O problems.
 */
auto BlockCache::sync() -> void
{
  for (auto &bp : blocks) {
    if (bp->isDirty()) {
      bp->write(dataSource);
    }
  }
}


}
