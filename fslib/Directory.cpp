// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#include "BlockCache.h"
#include "Directory.h"
#include "FilesystemException.h"
#include "Rad50.h"

#include <cassert>
#include <cerrno>
#include <ctime>

using std::min;
using std::string;
using std::unique_ptr;
using std::vector;

namespace RT11FS {
using namespace Dir;

/** 
 * Constructor for a directory.
 *
 * Does basic validation on the directory and will throw an exception on any errors.
 *
 * @param cache the block cache of the volume containing the directory data.
 */
Directory::Directory(BlockCache *cache)
  : cache(cache)
  , dirblk(nullptr)
{
  auto max_seg = (cache->getVolumeSectors() - FIRST_SEGMENT_SECTOR) / SECTORS_PER_SEGMENT;

  dirblk = cache->getBlock(FIRST_SEGMENT_SECTOR, 1);
  auto totseg = dirblk->extractWord(TOTAL_SEGMENTS);

  // basic sanity check
  if (totseg >= max_seg) {    
    throw FilesystemException {-EINVAL, "directory segments invalid"};
  }

  cache->resizeBlock(dirblk, totseg * SECTORS_PER_SEGMENT);

  auto extra = dirblk->extractWord(EXTRA_BYTES);
  auto segment = 1;

  // sanity: the extra bytes word had better be the same across all dir segments
  // (it's an attribute set when the directory is created.)
  while (segment) {
    auto base = (segment - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
    auto thisExtra = dirblk->extractWord(base + EXTRA_BYTES);
    if (thisExtra != extra) {
      throw FilesystemException {-EINVAL, "directory segments are not consistent"};
    }

    segment = dirblk->extractWord(base + NEXT_SEGMENT);
    if (segment > totseg) {
      throw FilesystemException {-EINVAL, "directory segment list is corrupt"};
    }
  }

  entrySize = ENTRY_LENGTH + extra;
}

/** 
 * Destructor for a directory
 */
Directory::~Directory()
{
  if (dirblk) {
    cache->putBlock(dirblk);
  }
}

/** 
 * Retrieve the directory entry for a named file
 *
 * Scans the directory looking for the named file and fills in `ent'.
 * Will return entries for any object other than end of segment (such as 
 * temporary entries.)
 * 
 * @param name the name of the file to search for.
 * @param ent on success, the directory entry for `name'.
 * @return 0 on success or a negated errno.
 * @retval -EINVAL if the filename cannot be parsed
 * @retval -ENOENT if the filename is not found in the directory
 */
auto Directory::getEnt(const std::string &name, DirEnt &ent) -> int
{
  auto rad50Name = Rad50Name {};
  if (!parseFilename(name, rad50Name)) {
    return -EINVAL;
  }

  auto dirp = getDirPointer(rad50Name);

  return getEnt(dirp, ent) ? 0 : -ENOENT;
}

/** 
 * Retrieve the directory entry for a named file
 *
 * Scans the directory looking for the named file and returns a pointer to it.
 * Will return entries for any object other than end of segment or free space 
 * (such as temporary entries.)
 * 
 * @param name the name of the file to search for.
 * @param dirp on success, the directory pointer for `name'.
 * @return 0 on success or a negated errno.
 * @retval -EINVAL if the filename cannot be parsed
 * @retval -ENOENT if the filename is not found in the directory
 */
auto Directory::getDirPointer(const std::string &name, unique_ptr<DirPtr> &dirpp) -> int
{
  auto rad50Name = Rad50Name {};
  if (!parseFilename(name, rad50Name)) {
    return -EINVAL;
  }

  auto dirp = startScan();

  while (++dirp) {
    if (dirp.hasStatus(E_MPTY) || dirp.hasStatus(E_EOS)) {
      continue;
    }

    if (
      rad50Name[0] == dirp.getWord(FILENAME_WORDS) &&
      rad50Name[1] == dirp.getWord(FILENAME_WORDS + 2) &&
      rad50Name[2] == dirp.getWord(FILENAME_WORDS + 4)
    ) {
      dirpp.reset(new DirPtr {dirp});
      return 0;
    }
  }

  return -ENOENT;
}

/** 
 * Gets a directory pointer to the named file.
 *
 * Returns the entry for any file status except end of segment.
 * TODO this should probably be tighter.
 *
 * @param name the Rad50 representation of the filename.
 * @return a pointer to the entry, or a pointer past the end of the directory if
 * the file does not exist.
 */
auto Directory::getDirPointer(const Dir::Rad50Name &name) -> DirPtr
{
  auto ds = startScan();

  while (++ds) {
    if (ds.hasStatus(E_EOS)) {
      continue;
    }

    if (
      name[0] == ds.getWord(FILENAME_WORDS) &&
      name[1] == ds.getWord(FILENAME_WORDS + 2) &&
      name[2] == ds.getWord(FILENAME_WORDS + 4)
    ) {
      break;
    }
  }

  return ds;
}

static auto rtrim(const string &str)
{
  int i = str.size() - 1;
  for (; i >= 0; i--) {
    if (str[i] != ' ') {
      break;
    }
  }

  return str.substr(0, i + 1);
}

/** 
 * Retrieve the directory entry from a directory pointer
 *
 * @param ptr A valid pointer into the directory
 * @param ent on success, the directory entry for `name'
 * @return true on success or false if the directory pointer is invalid
 */
auto Directory::getEnt(const DirPtr &ptr, DirEnt &ent) -> bool
{
  if (ptr.afterEnd()) {
    return false;
  }

  for (auto i = 0; i < ent.rad50_name.size(); i++) {
    ent.rad50_name[i] = ptr.getWord(FILENAME_WORDS + i * sizeof(uint16_t));
  }

  ent.name = Rad50::fromRad50(ent.rad50_name[0]);
  ent.name += Rad50::fromRad50(ent.rad50_name[1]);

  ent.name = rtrim(ent.name);
  ent.name += ".";
  
  ent.name += Rad50::fromRad50(ent.rad50_name[2]);
  ent.name = rtrim(ent.name);

  ent.status = ptr.getWord(STATUS_WORD);
  ent.length = ptr.getWord(TOTAL_LENGTH_WORD) * Block::SECTOR_SIZE;
  ent.sector0 = ptr.getDataSector();

  struct tm tm;
  memset(&tm, 0, sizeof(tm));

  auto dateWord = ptr.getWord(CREATION_DATE_WORD);
  
  // on an invalid date, tm will remain empty
  dirTimeToTime(dateWord, tm);

  ent.create_time = mktime(&tm);
  return true;
}

/** 
 * Starts a scan of the directory
 *
 * The returned directory pointer will be pointed just before the first entry
 * and must therefore be incremented before being dereferenced.
 *
 * @return an initialized directory pointer
 */
auto Directory::startScan() -> DirPtr
{
  return DirPtr {dirblk};
}

/** 
 * Scan for a directory entry with a specified bit set
 *
 * The pointer will be moved through the directory until an entry is found
 * that has *any* of the bits in `mask' set in the status word.
 *
 * @param ptr a pointer that will be advanced to a matching entry
 * @param mask a mask of bits to search for in the status word
 *
 * @return success
 */
auto Directory::moveNextFiltered(DirPtr &ptr, uint16_t mask) -> bool
{
  while (++ptr) {
    if ((ptr.getWord(STATUS_WORD) & mask) != 0) {
      return true;
    }
  }

  return false;
}

/**
 * Returns metadata about the file system
 * 
 * @param vfs the struct to fill with data about the volume's file system.
 * @return 0 for success or a negated errno.
 */
auto Directory::statfs(struct statvfs *vfs) -> int
{
  memset(vfs, 0, sizeof(struct statvfs));
  vfs->f_bsize = Block::SECTOR_SIZE;
  vfs->f_frsize = Block::SECTOR_SIZE;
  vfs->f_namemax = 10;

  auto segs = dirblk->extractWord(TOTAL_SEGMENTS);
  auto xtra = dirblk->extractWord(EXTRA_BYTES);
  // -1 because we have to leave space for the end of segment marker
  auto perseg = (SECTORS_PER_SEGMENT * Block::SECTOR_SIZE - FIRST_ENTRY_OFFSET) / (ENTRY_LENGTH + xtra) - 1;
  auto inodes = perseg * segs;

  vfs->f_blocks = cache->getVolumeSectors() - (FIRST_SEGMENT_SECTOR + segs * SECTORS_PER_SEGMENT);

  auto ptr = startScan();
    
  auto usedinodes = 0;
  auto usedblocks = 0;
  auto freeblocks = 0;

  while (++ptr) {
    auto status = ptr.getWord(STATUS_WORD);
    auto length = ptr.getWord(TOTAL_LENGTH_WORD);

    if ((status & E_MPTY) != 0) {
      freeblocks += length;
    } else {
      usedblocks += length;
      if ((status & E_EOS) == 0) {
        usedinodes++;
      }
    }
  }

  vfs->f_bfree = freeblocks;
  vfs->f_bavail = freeblocks;
  vfs->f_files = inodes;
  vfs->f_ffree = inodes - usedinodes;
  vfs->f_favail = vfs->f_ffree;

  return 0;
}

/**
 * Truncate a directory entry to a new size.
 *
 * This call is a bit misnamed for historical reasons. Truncate implies the entry can only shrink,
 * but this call can also cause it to grow.
 *
 * This call can cause entries to move around in the directory, on either a grow or a shrink
 * operation. On a grow operation, if can also require the movement of data sectors as well.
 * This is a side effect of the fact that the data sectors of a file in RT-11 are always
 * contiguous.
 *
 * @param dirp the directory entry to change.
 * @param newSize the new size, in bytes, of the entry.
 * @param moves a vector which will, on success, record how file entries were moved 
 * @return 0 on success or a negated errno
 */
auto Directory::truncate(DirPtr &dirp, off_t newSize, vector<DirChangeTracker::Entry> &moves) -> int
{
  auto tracker = DirChangeTracker {};

  // Express size in sectors
  newSize = (newSize + Block::SECTOR_SIZE - 1) / Block::SECTOR_SIZE;
  auto oldSize = dirp.getWord(TOTAL_LENGTH_WORD);

  if (newSize == oldSize) {
    return 0;
  }

  auto err = 0;
  if (newSize < oldSize) {
    err = shrinkEntry(dirp, newSize, tracker);
  } else {
    err = growEntry(dirp, newSize, tracker);
  }

  if (err < 0) {
    return err;
  }

  moves.clear();
  auto gotMoves = tracker.getMoves();
  copy(begin(gotMoves), end(gotMoves), back_inserter(moves));

  return 0;
}

/**
 * Delete the given file.
 *
 * If the file is open, FUSE will take care of renaming it for us and
 * unlinking the name when the file is no longer open.
 * 
 * @param name the name of the file to delete.
 * @param moves a vector which will, on success, record how file entries were moved 
 * @return 0 on success or a negative errno
 */
auto Directory::removeEntry(const std::string &name, vector<DirChangeTracker::Entry> &moves) -> int
{
  auto dirp = unique_ptr<DirPtr> {};
  auto err = getDirPointer(name, dirp);
  if (err < 0) {
    return err;
  }

  auto tracker = DirChangeTracker {};

  // first, turn the file into free space
  dirp->setWord(STATUS_WORD, E_MPTY);
  for (auto i = 0; i < FILENAME_LENGTH; i++) {
    dirp->setWord(FILENAME_WORDS + 2*i, 0);
  }

  // combine free blocks
  coalesceNeighboringFreeBlocks(*dirp, tracker);

  moves.clear();
  auto gotMoves = tracker.getMoves();
  copy(begin(gotMoves), end(gotMoves), back_inserter(moves));

  return 0;
}

/**
 * Rename a file.
 *
 * If the file's new name is the name of an existing file, then the existing file
 * will be overwritten.
 *
 * TODO FUSE will rename files to unique names if open file object is overwritten.
 * The names it uses are not Rad50 safe. Translate them.
 * 
 * @param oldName is the name of the file to rename.
 * @param newName is the new name of the file.
 */
auto Directory::rename(const std::string &oldName, const std::string &newName) -> int
{
  auto oldRad50 = Rad50Name {};
  if (!parseFilename(oldName, oldRad50)) {
    return -EINVAL;
  }

  auto newRad50 = Rad50Name {};
  if (!parseFilename(newName, newRad50)) {
    return -EINVAL;
  }

  auto oldp = getDirPointer(oldRad50);
  auto newp = getDirPointer(newRad50);

  if (oldp.afterEnd()) {
    return -ENOENT;
  }

  if (!newp.afterEnd()) {
    // TODO unlink
  }

  for (auto i = 0; i < FILENAME_LENGTH; i++) {
    oldp.setWord(FILENAME_WORDS + 2 * i, newRad50[i]);
  }

  cache->sync();
  return 0;
}

/**
 * Create a new file.
 *
 * Create a new entry with the given name. The entry will be zero length and we will 
 * grow it as needed.
 *  
 * @param name the name of the file to create.
 * @param dirpp on success, set to the new entry.
 * @param moves a vector which will, on success, record how file entries were moved. 
 * @return 0 on success or a negative errno
 */
auto Directory::createEntry(const string &name, unique_ptr<DirPtr> &dirpp, vector<DirChangeTracker::Entry> &moves) -> int
{
  auto rad50Name = Rad50Name {};
  if (!parseFilename(name, rad50Name)) {
    return -EINVAL;
  }

  auto dirp = findLargestFreeBlock();
  if (dirp.afterEnd()) {
    return -ENOSPC;
  }

  auto tracker = DirChangeTracker {};

  // if there's another open file right before us, then we want to leave it 
  // space to grow. otherwise, just put the new entry right at the start of the 
  // free block.
  auto prev = dirp.prev();
  if (!prev.beforeStart() && prev.hasStatus(E_TENT)) {
    auto size = dirp.getWord(TOTAL_LENGTH_WORD) / 2;    

    auto err = carveFreeBlock(dirp, size, tracker);
    if (err < 0) {
      return err;
    }

    ++dirp;
    assert(!dirp.afterEnd());
    assert(dirp.hasStatus(E_MPTY));
  } 
    
  auto err = insertEmptyAt(dirp, tracker);
  if (err < 0) {
    return err;
  }

  // Build the new entry. It'll be a zero-sector tentative file.
  // The size is already set.
  dirp.setWord(STATUS_WORD, E_TENT);
  for (auto i = 0; i < FILENAME_LENGTH; i++) {
    dirp.setWord(FILENAME_WORDS + 2*i, rad50Name[i]);
  }

  auto now = time(nullptr);
  auto tm = localtime(&now);
  auto dirtime = uint16_t {0};

  timeToDirTime(*tm, dirtime);
  
  dirp.setWord(CREATION_DATE_WORD, dirtime);

  dirpp.reset(new DirPtr {dirp});

  return 0;
}

/**
 * If the entry is tentative (E_TENT) then make it a permanent file.
 *
 * An entry will be tentative if it was a new entry created with `createEntry'.
 *
 * @param dirp the entry to make permanent.
 */
auto Directory::makeEntryPermanent(DirPtr &dirp) -> void
{
  if (dirp.hasStatus(E_TENT)) {
    dirp.setWord(STATUS_WORD, E_PERM);    
  }
}

/**
 * Shrink the given entry.
 * 
 * It is expected that this call will never be used on a free space
 * block.
 *
 * If there is a free space block directly following `dirp', then
 * we can just add space into it. If not, then we have to insert
 * a directory slot for the free'd space.
 *
 * @param dirp points to the entry to shrink.
 * @param newSize the new size of the entry, in sectors.
 * @param tracker a tracker to log entry movement.
 * @return 0 on success or a negated errno (most commonly if
 * the entire directory is full.)
 * 
 */
auto Directory::shrinkEntry(DirPtr &dirp, int newSize, DirChangeTracker &tracker) -> int
{
  auto nextp = dirp.next();

  if (!nextp.hasStatus(E_MPTY)) {
    // if this succeeds, then nextp will point to a zero-sector
    // empty entry in the same location.
    auto err = insertEmptyAt(nextp, tracker);
    if (err) {
      // if we can't insert an empty entry, can't continue.
      return err;
    }
  }

  auto delta = dirp.getWord(TOTAL_LENGTH_WORD) - newSize;
  assert(delta > 0);
  dirp.setWord(TOTAL_LENGTH_WORD, newSize);
  nextp.setWord(TOTAL_LENGTH_WORD, nextp.getWord(TOTAL_LENGTH_WORD) + delta);
  return 0;
}

/**
 * Grow the given entry.
 * 
 * It is expected that this call will never be used on a free space
 * block.
 *
 * If there is a free space block directly following `dirp', then
 * steal space from it if it's big enough. Otherwise, search the
 * directory for a free block big enough to hold the new requested
 * file size. If there is none, then there aren't enough
 * contiguous free blocks and the volume should be compacted.
 *
 * @param dirp points to the entry to grow.
 * @param newSize the new size of the entry, in sectors.
 * @param tracker a tracker to log entry movement.
 * @return 0 on success or a negated errno (most commonly if
 * the entire directory is full.)
 * 
 */
auto Directory::growEntry(DirPtr &dirp, int newSize, DirChangeTracker &tracker) -> int
{
  auto next = dirp.next();
  if (next.hasStatus(E_MPTY)) {
    auto available = dirp.getWord(TOTAL_LENGTH_WORD) + next.getWord(TOTAL_LENGTH_WORD);
    if (newSize <= available) {
      // transfer size
      auto delta = newSize - dirp.getWord(TOTAL_LENGTH_WORD);
      dirp.setWord(TOTAL_LENGTH_WORD, newSize);
      next.setWord(TOTAL_LENGTH_WORD, next.getWord(TOTAL_LENGTH_WORD) - delta);
    

      if (next.getWord(TOTAL_LENGTH_WORD) == 0) {
        // delete empty free space entry
        deleteEmptyAt(next, tracker);
      }

      return 0;
    }
  }

  // we couldn't grow the file in place, so move it to a free block
  // large enough to contain it
  auto newp = findLargestFreeBlock();
  if (newp.afterEnd() || newp.getWord(TOTAL_LENGTH_WORD) < newSize) {
    return -ENOSPC;
  }

  auto name = Rad50Name {};
  name[0] = dirp.getWord(FILENAME_WORDS + 0);
  name[1] = dirp.getWord(FILENAME_WORDS + 2);
  name[2] = dirp.getWord(FILENAME_WORDS + 4);

  auto inserted = carveFreeBlock(newp, newSize, tracker);
  if (inserted < 0) {
    return inserted;
  }

  // we have to be careful here - carveFreeBlock may have inserted
  // a new entry before us in the same segment
  if (inserted == 1 && newp.getSegment() == dirp.getSegment() && newp.getIndex() < dirp.getIndex()) {
    // we can't just increment dirp because that'll mess up it's starting sector
    // we just need to adjust the index to match the fact that the entry moved 
    // within the segment
    dirp.incIndex();
  }

  // newp is now exactly the requested size

  // TODO this will trash any open file that has cached a directory entry 
  // to the file we're moving. Fix them up.
  //
  auto src = dirp.getDataSector();
  auto dst = newp.getDataSector();
  auto cnt = dirp.getWord(TOTAL_LENGTH_WORD);

  // Note that even if the cache is doing writethrough, this is safe to do
  // before the directory gets updated because we're just writing data into
  // the data area of a free block
  while (cnt--) {
    auto srcBlk = cache->getBlock(src++, 1);
    auto dstBlk = cache->getBlock(dst++, 1);
    dstBlk->copyFromOtherBlock(srcBlk, 0, 0, Block::SECTOR_SIZE);
    cache->putBlock(srcBlk);
    cache->putBlock(dstBlk);
  }

  moveEntryAcrossSegments(dirp, newp, tracker);

  // we just wrote over the new entry with the old size; put it back    
  newp.setWord(TOTAL_LENGTH_WORD, newSize);

  dirp.setWord(STATUS_WORD, E_MPTY);
  dirp.setWord(FILENAME_WORDS, 0);
  dirp.setWord(FILENAME_WORDS + 2, 0);
  dirp.setWord(FILENAME_WORDS + 4, 0);
  dirp.setByte(JOB_BYTE, 0);
  dirp.setByte(CHANNEL_BYTE, 0);
  dirp.setWord(CREATION_DATE_WORD, 0);

  coalesceNeighboringFreeBlocks(dirp, tracker);

  // point dirp at whever the original file landed
  dirp = getDirPointer(name);

  return 0;
}

/**
 * Insert a zero-sector free space directory entry at `dirp'.
 *
 * The entry will be created by moving everything else down.
 * If the segment containing `dirp' is full, then the last 
 * entry will spill into the next segment. This may happen
 * recursively.
 * 
 * @param dirp a pointer to the entry that will be free space 
 * upon success.
 * @param tracker a tracker to record entry movement
 * @return 0 on success or a negative errno
 */
auto Directory::insertEmptyAt(DirPtr &dirp, DirChangeTracker &tracker) -> int
{
  auto eos = advanceToEndOfSegment(dirp);

  if (eos.getIndex() >= maxEntriesPerSegment() - 1) {
    auto err = spillLastEntry(dirp, tracker);
    if (err) {
      return err;
    }

    // NOTE that the previous operation will have changed the position
    // of the end of the segment
    eos = advanceToEndOfSegment(dirp);
    assert(eos.getSegment() < maxEntriesPerSegment() - 1);
  }

  // at this point we know it's safe to move everything down by
  // one entry
  auto destp = dirp;
  destp.incIndex();   // don't let this go to next segment if we're right on EOS
  auto cnt = eos.getIndex() - dirp.getIndex() + 1;
  moveEntriesWithinSegment(dirp, destp, cnt, tracker);

  // the contents of dirp have been moved, so we can 
  // now fill dirp with the empty free entry
  dirp.setWord(STATUS_WORD, E_MPTY);
  dirp.setWord(FILENAME_WORDS, 0);
  dirp.setWord(FILENAME_WORDS + 2, 0);
  dirp.setWord(FILENAME_WORDS + 4, 0);
  dirp.setWord(TOTAL_LENGTH_WORD, 0);
  dirp.setByte(JOB_BYTE, 0);
  dirp.setByte(CHANNEL_BYTE, 0);
  dirp.setWord(CREATION_DATE_WORD, 0);

  return 0;
}

/**
 * Delete a zero-sector free space directory entry at `dirp'.
 *
 * The entry must be zero size, because otherwise the sector
 * addresses of the following files would be incorrect.
 * Deleting a non-empty file requires turning the file's 
 * entry into free space, which is a different task.
 * 
 * @param dirp a pointer to the entry that will be free space 
 * upon success.
 * @param tracker a tracker to log entry movement.
 * @return 0 on success or a negative errno
 */
auto Directory::deleteEmptyAt(DirPtr &dirp, DirChangeTracker &tracker) -> void
{
  assert(dirp.getWord(TOTAL_LENGTH_WORD) == 0);

  auto eos = advanceToEndOfSegment(dirp);

  auto srcp = dirp;
  srcp.incIndex();
  auto cnt = eos.getIndex() - srcp.getIndex() + 1;
  moveEntriesWithinSegment(srcp, dirp, cnt, tracker);
}

/**
 * Move the last entry in the pointed-to segment to the next segment.
 *
 * The last entry is the enter just before the end of segment marker. If
 * there is no other entry in the segment (i.e. just the end of segment marker)
 * then nothing is done.
 *
 * If the next semgent is totally full, then it will in turn be spilled to 
 * the next next segment and so on. If the last segment is reached in this
 * manner then a new segment will be allocated. If all segments are full
 * then the operation will fail.
 *
 * @param dirp points to the segment to move the last entry out of
 * @param tracker a tracker to log entry moves
 * @return 0 on success or a negated errno
 */
auto Directory::spillLastEntry(const DirPtr &dirp, DirChangeTracker &tracker) -> int
{
  auto eos = advanceToEndOfSegment(dirp);

  if (eos.getIndex() == 0) {
    // can't spill entry if there aren't any
    return 0;
  }

  auto next = eos.next();
  if (next.afterEnd()) {
    // there is no next segment yet... allocate one
    auto err = allocateNewSegment();
    if (err) {
      return err;
    }

    // if the previous operation succeeded, then there must be a next
    // segment now
    next = eos.next();
    assert(!next.afterEnd());
  } 

  auto last = eos.prev();
  assert(last.getDataSector() + last.getWord(TOTAL_LENGTH_WORD) == next.getDataSector());

  // this will take care of recusively spilling if next's segment is full  
  auto err = insertEmptyAt(next, tracker);
  if (err) {
    return err;
  }

  // since `next' is one past an eos marker, it must be the first entry
  // in a segment.
  assert(next.getIndex() == 0);

  // move last entry to next segment
  moveEntryAcrossSegments(last, next, tracker);

  next.setSegmentWord(SEGMENT_DATA_BLOCK, last.getDataSector());

  // now we can mark last as end of segment
  last.setWord(STATUS_WORD, E_EOS);

  // RT-11 doesn't bother with clearing the filename  
  last.setWord(FILENAME_WORDS, 0);
  last.setWord(FILENAME_WORDS + 2, 0);
  last.setWord(FILENAME_WORDS + 4, 0);

  last.setWord(TOTAL_LENGTH_WORD, 0);

  return 0;
}

/**
 * Add a new segment to the end of the directory.
 *
 * The new entry will contain just an end of segment marker.
 * 
 * @return 0 on success or a negated errno
 */
auto Directory::allocateNewSegment() -> int
{
  // TODO figure out if RT11 will ever free a segment in the middle
  // of the list (i.e. can there ever be a gap)
  // if so we should traverse the segment list to figure this out
  auto next = 1 + dirblk->extractWord(HIGHEST_SEGMENT);
  if (next > dirblk->extractWord(TOTAL_SEGMENTS)) {
    return -ENOSPC;
  }

  // find the last entry (which also gives us the last segment)
  auto eos = startScan();
  while (true) {
    auto nextp = eos.next();
    if (nextp.afterEnd()) {
      break;
    }
    eos = nextp;
  }

  // unused segments are not initialized yet
  int header = (next - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
  dirblk->setWord(header + TOTAL_SEGMENTS, dirblk->extractWord(TOTAL_SEGMENTS));
  dirblk->setWord(header + NEXT_SEGMENT, 0);
  dirblk->setWord(header + HIGHEST_SEGMENT, 0);   // per docs, only set in segment 1
  dirblk->setWord(header + EXTRA_BYTES, dirblk->extractWord(EXTRA_BYTES));
  dirblk->setWord(header + SEGMENT_DATA_BLOCK, eos.getDataSector());

  int entry0 = header + FIRST_ENTRY_OFFSET;
  dirblk->setWord(entry0 + STATUS_WORD, E_EOS);
  dirblk->setWord(entry0 + FILENAME_WORDS, 0);
  dirblk->setWord(entry0 + FILENAME_WORDS + 2, 0);
  dirblk->setWord(entry0 + FILENAME_WORDS + 4, 0);
  dirblk->setWord(entry0 + TOTAL_LENGTH_WORD, 0);
  dirblk->setByte(entry0 + JOB_BYTE, 0);
  dirblk->setByte(entry0 + CHANNEL_BYTE, 0);
  dirblk->setWord(entry0 + CREATION_DATE_WORD, 0);

  // `next' is now a valid segment and we can link it i
  eos.setSegmentWord(NEXT_SEGMENT, next);

  dirblk->setWord(HIGHEST_SEGMENT, next);

  return 0;
}

/**
 * Find the largest free block in the directory.
 *
 * @return a directory pointer to the free block, which will be afterEnd 
 * if there is no free block.
 */
auto Directory::findLargestFreeBlock() -> DirPtr
{
  auto largestBlock = -1;
  auto largestBlockPtr = DirPtr {dirblk};

  auto dirp = startScan();

  while (++dirp) {
    if (!dirp.hasStatus(E_MPTY)) {
      continue;
    }

    auto length = dirp.getWord(TOTAL_LENGTH_WORD);
    if (length > largestBlock) {
      largestBlock = length;
      largestBlockPtr = dirp;
    }
  }

  if (largestBlock <= 0) {
    // dirp is at the end now
    largestBlockPtr = dirp;
  }

  return largestBlockPtr;
}

/**
 * Carve a smaller block out of a free block
 *
 * The block must be large enough to contain `size' sectors.
 * A new directory entry must probably be created; this method
 * can fail if a there's no room for another entry after 
 * the existing free block.
 *
 * On return, `dirp' is still valid and points to a free 
 * block of exactly `size' sectors.
 *
 * @param dirp the free block to split
 * @param size the new size in sectors of `dirp'
 * @param tracker a tracker to log entry movement
 * @return the number of blocks inserted (>= 0) or a negated errno
 */
auto Directory::carveFreeBlock(DirPtr &dirp, int size, DirChangeTracker &tracker) -> int
{
  if (size > dirp.getWord(TOTAL_LENGTH_WORD)) {
    return -EINVAL;
  }

  // if the new block is larger than we need, carve it up
  if (size < dirp.getWord(TOTAL_LENGTH_WORD)) {
    auto next = dirp.next();
    auto err = insertEmptyAt(next, tracker);
    if (err) {
      return err;
    }

    auto delta = dirp.getWord(TOTAL_LENGTH_WORD) - size;
    dirp.setWord(TOTAL_LENGTH_WORD, dirp.getWord(TOTAL_LENGTH_WORD) - delta);
    next.setWord(TOTAL_LENGTH_WORD, delta);

    return 1;
  }

  return 0;
}

/**
 * Combine all free blocks on either side of the current block.
 *
 * The current block is expected to be a free block itself.
 *
 * @param ptr a free block to combine
 * @param tracker a tracker to log entry movement
 */
auto Directory::coalesceNeighboringFreeBlocks(DirPtr &ptr, DirChangeTracker &tracker) -> void
{
  if (!ptr.hasStatus(E_MPTY)) {
    return;
  }

  auto first = ptr;
  while (true) {
    auto prev = first.prev();
    if (prev.beforeStart() || !prev.hasStatus(E_MPTY)) {
      break;
    }

    first = prev;
  }

  while (true) {
    auto next = first.next();
    if (next.afterEnd() || !next.hasStatus(E_MPTY)) {
      break;
    }

    auto len = first.getWord(TOTAL_LENGTH_WORD) + next.getWord(TOTAL_LENGTH_WORD);

    first.setWord(TOTAL_LENGTH_WORD, len);
    next.setWord(TOTAL_LENGTH_WORD, 0);

    deleteEmptyAt(next, tracker);
  }
}

/**
 * Compute the maximum number of entries that will fit in one segment.
 *
 * This number is variable because, although a segment is always 1k,
 * RT-11 allows a volume to be formatted with extra space on each entry
 * for application use. 
 *
 * @return the number of entries per segment on this volume
 */
auto Directory::maxEntriesPerSegment() const -> int
{
  return (Block::SECTOR_SIZE * SECTORS_PER_SEGMENT - FIRST_ENTRY_OFFSET) / entrySize;
}

/**
 * Given a directory pointer, return another pointer that points to
 * the end of segment marker in the same segment.
 *
 * The original pointer is not modified.
 *
 * @return a pointer to the end of segment marker
 */
auto Directory::advanceToEndOfSegment(const DirPtr &dirp) -> DirPtr
{
  auto eos = dirp;

  while (!eos.hasStatus(E_EOS)) {
    ++eos;
  }

  return eos;
}

/**
 * Move directory entries around in one segment.
 *
 * All entries in the source and destination ranges must fit in one segemnt.
 * This routine will not spill into the next segment.
 * 
 * @param segment the segment to reorder.
 * @param sourceIdx the first entry in the source range.
 * @param destIdx the first entry in the destination range.
 * @param count the number of entries to move.
 */
auto Directory::moveEntriesWithinSegment(const DirPtr &src, const DirPtr &dst, int count, DirChangeTracker &tracker) -> void
{
  assert(src.getSegment() == dst.getSegment());
  assert(count > 0 && count <= maxEntriesPerSegment());
  assert(src.getIndex() + count <= maxEntriesPerSegment());
  assert(dst.getIndex() + count <= maxEntriesPerSegment());

  tracker.beginTransaction();
  auto s = src;
  auto d = dst;
  for (auto c = count; c--; s++, d++) {
    tracker.moveDirEntry(s, d);
  }
  tracker.endTransaction();

  auto srcOffset = src.offset(0);
  auto dstOffset = dst.offset(0);

  dirblk->copyWithinBlock(srcOffset, dstOffset, count * entrySize);
}

/**
 * Move one directory entry. May move between segments.
 *  
 * @param src the source entry.
 * @param dst the slot to move the entry to.
 */
auto Directory::moveEntryAcrossSegments(const DirPtr &src, const DirPtr &dst, DirChangeTracker &tracker) -> void
{
  auto srcOffset = src.offset(0);
  auto dstOffset = dst.offset(0);

  tracker.beginTransaction();
  tracker.moveDirEntry(src, dst);
  tracker.endTransaction();

  dirblk->copyWithinBlock(srcOffset, dstOffset, entrySize);
}

/**
 * Parse a filename into RT11 RAD50 representation
 *
 * The filename must contain from 1 to 6 RAD50 characters. Optionally,
 * it may be suffixed with a dot and an extension (0 to 3 RAD50 characters).
 * The file system is case sensitive and lower case values are not in
 * the RAD50 character set, so lower case filenames will not parse.
 * 
 * @param name the filename to parse
 * @param rad50 on success, will contain the parsed filename
 * @return true on success
 */
auto Directory::parseFilename(const std::string &name, Rad50Name &rad50) -> bool
{
  auto base = string {};
  auto ext = string {};

  auto n = name.find('.');
  if (n != string::npos) {
    base = name.substr(0, n);
    ext = name.substr(n + 1);
  } else {
    base = name;
  }

  if (base.size() > 6 || ext.size() > 3) {
    return false;
  }

  base = (base + "      ").substr(0, 6);
  ext = (ext + "   ").substr(0, 3);

  if (
    !Rad50::toRad50(base.substr(0, 3), rad50[0]) ||
    !Rad50::toRad50(base.substr(3, 3), rad50[1]) ||
    !Rad50::toRad50(ext, rad50[2])
  ) {
    return false;
  }

  return true;
}

/** 
 * Convert an RT-11 directory timestamp to a tm
 *
 * @param dirTime the time to convert
 * @param tm on success, `dirTime' as a tm struct
 * @return true on success
 */
auto Directory::dirTimeToTime(uint16_t dirTime, struct tm &tm) -> bool
{
  auto year = dirTime & 0b0000000000011111;
  auto day = (dirTime & 0b0000001111100000) >> 5;
  auto mon = (dirTime & 0b0011110000000000) >> 10;
  auto age = (dirTime & 0b1100000000000000) >> 14;


  if (mon < 1 || mon > 12) {
    return false;
  }

  auto monthLengths = vector<int>{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  auto leap = (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
  if (leap) {
    monthLengths[2]++;
  }

  if (day < 1 || day > monthLengths.at(mon)) {
    return false;
  }

  year = 1972 + year + age * 32;

  memset(&tm, 0, sizeof(struct tm));
  tm.tm_year = year - 1900;
  tm.tm_mon = mon - 1;
  tm.tm_mday = day;

  return true;
}

/**
 * Convert a struct tm to an RT-11 directory timestamp
 *
 * @param tm the time to convert.
 * @param dirtime on success, the converted time.
 * @return true on success, false if the timestamp was invalid
 */
auto Directory::timeToDirTime(const struct tm &tm, uint16_t &dirTime) -> bool
{    
  auto monthLengths = vector<int>{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  auto year = tm.tm_year + 1900;
  auto leap = (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
  if (leap) {
    monthLengths[2]++;
  }

  if (tm.tm_mon < 0 || tm.tm_mon > 11) {
    return false;
  }

  if (tm.tm_mday < 1 || tm.tm_mday > monthLengths.at(tm.tm_mon + 1)) {
    return false;
  }

  // the maximum representable year based on the bitfields in a timestamp
  // (works out to 2099).
  const auto maxYear =  1972 + 4 * 32 + 31;
  if (year > maxYear) {
    return false;
  }

  year -= 1972;
  auto age = year / 32;
  year = year - age * 32;

  auto mon = 1 + tm.tm_mon;
  auto day = tm.tm_mday;

  dirTime = 
    ((age << 14) & 0b1100000000000000) |
    ((mon << 10) & 0b0011110000000000) |
    ((day <<  5) & 0b0000001111100000) |
    (year        & 0b0000000000011111);

  return true;
}


}
