#include "BlockCache.h"
#include "Directory.h"
#include "FilesystemException.h"
#include "Rad50.h"

#include <cassert>
#include <cerrno>

using std::string;

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
    auto base = (FIRST_ENTRY_OFFSET + (segment - 1) * SECTORS_PER_SEGMENT) * Block::SECTOR_SIZE;
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
 * Scans the directory looking for the named file and fills in `ent'.
 * Will return entries for any object other than end of segment (such as 
 * temporary entries.)
 * 
 * @param name the name of the file to search for, in Rad50
 * @param ent on success, the directory entry for `name'.
 * @return 0 on success or a negated errno.
 * @retval -EINVAL if the filename cannot be parsed
 * @retval -ENOENT if the filename is not found in the directory
 */
auto Directory::getDirPointer(const Dir::Rad50Name &name) -> DirPtr
{
  auto ds = startScan();

  while (++ds) {
    auto status = ds.getWord(STATUS_WORD);
    if ((status & E_EOS) != 0) {
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

  auto age = (dateWord >> 14) & 03;  
  tm.tm_mon = ((dateWord >> 10) & 017) - 1;
  tm.tm_mday = (dateWord >> 5) & 037;
  tm.tm_year = 72 + age * 32 + (dateWord & 037);

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
  auto perseg = (Block::SECTOR_SIZE - FIRST_ENTRY_OFFSET) / (ENTRY_LENGTH + xtra);
  auto inodes = perseg * segs;

  vfs->f_blocks = cache->getVolumeSectors() - (FIRST_SEGMENT_SECTOR + segs * SECTORS_PER_SEGMENT);

  auto ptr = startScan();
  auto ent = DirEnt {};
    
  auto usedinodes = 0;
  auto usedblocks = 0;
  auto freeblocks = 0;

  while (++ptr) {
    getEnt(ptr, ent);
    if ((ent.status & Dir::E_MPTY) != 0) {
      freeblocks += ent.length;
    } else {
      usedblocks += ent.length;
      usedinodes++;
    }
  }

  vfs->f_bfree = freeblocks;
  vfs->f_bavail = freeblocks;
  vfs->f_files = inodes;
  vfs->f_ffree = inodes - usedinodes;
  vfs->f_favail = vfs->f_ffree;

  return 0;
}

auto Directory::truncate(const DirEnt &de, off_t newSize) -> int
{
  auto dirp = getDirPointer(de.rad50_name);
  if (dirp.afterEnd()) {
    return -ENOENT;
  }

  // Express size in sectors
  newSize = (newSize + Block::SECTOR_SIZE - 1) / Block::SECTOR_SIZE;
  auto oldSize = dirp.getWord(TOTAL_LENGTH_WORD);

  if (newSize == oldSize) {
    return 0;
  }

  if (newSize < oldSize) {
    // shrink the entry
  } else {
    // grow the entry
  }

  return 0;
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
 * @return 0 on success or a negated errno (most commonly if
 * the entire directory is full.)
 * 
 */
auto Directory::shrinkEntry(DirPtr &dirp, int newSize) -> int
{
  auto nextp = dirp.next();

  if ((nextp.getWord(STATUS_WORD) & E_MPTY) == 0) {
    // if this succeeds, then nextp will point to a zero-sector
    // empty entry in the same location.
    auto err = insertEmptyAt(nextp);
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
 * Insert a zero-sector free space directory entry at `dirp'.
 *
 * The entry will be created by moving everything else down.
 * If the segment containing `dirp' is full, then the last 
 * entry will spill into the next segment. This may happen
 * recursively.
 * 
 * @param dirp a pointer to the entry that will be free space 
 * upon success.
 * @return 0 on success or a negative errno
 */
auto Directory::insertEmptyAt(DirPtr &dirp) -> int
{
  auto eos = advanceToEndOfSegment(dirp);

  if (eos.getSegment() >= maxEntriesPerSegment() - 1) {
    auto err = spillLastEntry(dirp);
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
  auto src = dirp.offset(0);
  auto dst = src + entrySize;
  auto cnt = eos.offset(0) - src + entrySize;
  dirblk->copyWithinBlock(src, dst, cnt);

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
 * Move the last enty in the pointed-to segment to the next segment.
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
 * @return 0 on success or a negated errno
 */
auto Directory::spillLastEntry(const DirPtr &dirp) -> int
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
  auto err = insertEmptyAt(next);
  if (err) {
    return err;
  }

  assert(next.getIndex() == 0);

  // move last entry to next segment
  dirblk->copyWithinBlock(
    last.offset(0),
    next.offset(0),
    entrySize);

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
  int header = (FIRST_SEGMENT_SECTOR + (next - 1) * SECTORS_PER_SEGMENT) * Block::SECTOR_SIZE;
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

  return 0;
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

  while ((eos.getWord(STATUS_WORD) & E_EOS) == 0) {
    ++eos;
  }

  return eos;
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

}