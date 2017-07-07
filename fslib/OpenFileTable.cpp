#include "BlockCache.h"
#include "DirConst.h"
#include "Directory.h"
#include "OpenFileTable.h"

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <memory>

using std::cerr;
using std::endl;
using std::find_if;
using std::min;
using std::unique_ptr;
using std::vector;

namespace RT11FS {

/**
 * Create an OpenFileTable around the given directory.
 *
 * @param directory the directory containing the files.
 * @param cache the cache backing the volume containing the file system.
 */
OpenFileTable::OpenFileTable(Directory *dir, BlockCache *cache)
  : directory(dir)
  , cache(cache)
{
}

/** 
 * Create a file reference.
 *
 * File references are small integers. If there is already an entry
 * for the given file, then that entry will be returned. Otherwise,
 * a new entry will be created.
 *
 * @param name the name of the file to open.
 * @return A file reference or a negative errno on failure
 */
auto OpenFileTable::openFile(const std::string &name) -> int
{
  auto dirpp = unique_ptr<DirPtr> {};

  auto err = directory->getDirPointer(name, dirpp);
  if (err < 0) {
    cerr << "failed to open " << name << endl;
    return err;
  }

  auto iter = find_if(begin(openFiles), end(openFiles), [&dirpp](auto &p) {
    return dirpp->getSegment() == p.dirp.getSegment() && dirpp->getIndex() == p.dirp.getIndex();
  });

  if (iter != end(openFiles)) {
    iter->refcnt++;
    return iter - begin(openFiles);
  }

  auto entry = OpenFileEntry {
    .dirp = *dirpp,
    .refcnt = 1
  };

  iter = find_if(begin(openFiles), end(openFiles), [](auto &p) {
    return p.refcnt == 0;
  });

  auto index = openFiles.size();
  if (iter != end(openFiles)) {
    index = iter - begin(openFiles);
    openFiles[index] = entry;
  } else {
    openFiles.push_back(entry);  
  }

  cerr << "openfile: fd " << index << endl;

  return index;
}

/**
 * Release a file reference.
 *
 * If the last reference to a file is released, then the entry will be marked
 * available.
 *
 * @param fd the file descriptor to release.
 * @return 0 on success or a negative errno
 */
auto OpenFileTable::closeFile(int fd) -> int
{
  if (openFiles.at(fd).refcnt <= 0) {
    return -EINVAL;
  }

  openFiles.at(fd).refcnt--;
  return 0;
}

/**
 * Read data from a file.
 *
 * @param fd the file to read from.
 * @param buffer the buffer to read into.
 * @param count the number of bytes to read.
 * @param offset the offset in the file to read into.
 * @return The number of bytes read or a negative errno
 */
auto OpenFileTable::readFile(int fd, char *buffer, size_t count, off_t offset) -> int
{
  if (openFiles.at(fd).refcnt <= 0) {
    return -EINVAL;
  }

  const auto &dirp = openFiles.at(fd).dirp;

  auto fileLength = dirp.getWord(Dir::TOTAL_LENGTH_WORD);
  auto sector0 = dirp.getDataSector();
  auto end = offset + count;
  auto got = int {0};

  while (offset < end) {
    auto sector = offset / Block::SECTOR_SIZE;
    if (sector >= fileLength) {
      break;
    }

    auto secoffs = offset % Block::SECTOR_SIZE;
    auto blk = cache->getBlock(sector0 + sector, 1);

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

/**
 * Write data to a file.
 *
 * @param fd the file to write to.
 * @param buffer the buffer to write from.
 * @param count the number of bytes to write.
 * @param offset the offset in the file to write to.
 * @return The number of bytes written or a negative errno
 */
auto OpenFileTable::writeFile(int fd, const char *buffer, size_t count, off_t offset) -> int
{
  if (openFiles.at(fd).refcnt <= 0) {
    return -EINVAL;
  }

  auto &dirp = openFiles.at(fd).dirp;

  auto end = offset + count;
  auto got = int {0};
  auto length = dirp.getWord(Dir::TOTAL_LENGTH_WORD) * Block::SECTOR_SIZE;
  auto extendFile = end > length;

  if (extendFile) {
    auto moves = vector<DirChangeTracker::Entry> {};
    auto err = directory->truncate(dirp, end, moves);
    if (err < 0) {
      return err;
    }

    applyMoves(moves);
    dirp = openFiles.at(fd).dirp;
  }

  while (offset < end) {
    auto sector = offset / Block::SECTOR_SIZE;
    auto secoffs = offset % Block::SECTOR_SIZE;
    auto blk = cache->getBlock(dirp.getDataSector() + sector, 1);

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

auto OpenFileTable::truncate(int fd, off_t newSize) -> int
{
  if (openFiles.at(fd).refcnt <= 0) {
    return -EINVAL;
  }
  auto &dirp = openFiles.at(fd).dirp;

  auto moves = vector<DirChangeTracker::Entry> {};
  auto err = directory->truncate(dirp, newSize, moves);
  if (err < 0) {
    return err;
  }

  applyMoves(moves);
  return err;
}

auto OpenFileTable::applyMoves(const std::vector<DirChangeTracker::Entry> &moves) -> void
{

}

}