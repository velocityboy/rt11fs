#include "BlockCache.h"
#include "Directory.h"
#include "FilesystemException.h"
#include "Rad50.h"

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

  auto ds = startScan();

  while (++ds) {
    auto status = ds.getWord(STATUS_WORD);
    if ((status & E_EOS) != 0) {
      continue;
    }

    if (
      rad50Name[0] == ds.getWord(FILENAME_WORDS) &&
      rad50Name[1] == ds.getWord(FILENAME_WORDS + 2) &&
      rad50Name[2] == ds.getWord(FILENAME_WORDS + 4)
    ) {
      break;
    }
  }

  return getEnt(ds, ent) ? 0 : -ENOENT;
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