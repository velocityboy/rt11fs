#include "BlockCache.h"
#include "Directory.h"
#include "FilesystemException.h"
#include "Rad50.h"

#include <cerrno>

using std::string;

namespace {
// segment header on disk 
const uint16_t TOTAL_SEGMENTS = 0;        // total segments allocated for directory
const uint16_t NEXT_SEGMENT = 2;          // 1-based index of next segment
const uint16_t HIGHEST_SEGMENT = 4;       // highest segment in use (only maintained in segment 1)
const uint16_t EXTRA_BYTES = 6;           // extra bytes at the end of each dir entry
const uint16_t SEGMENT_DATA_BLOCK = 8;    // first disk block of first file in segment
const uint16_t FIRST_ENTRY_OFFSET = 10;   // offset of first entry in segment

// directory entry on disk 
const uint16_t STATUS_WORD = 0;           // offset of status word in entry
const uint16_t FILENAME_WORDS = 2;        // offset of filename (3x rad50 words)
const uint16_t TOTAL_LENGTH_WORD = 8;     // offset of file length (in blocks)
const uint16_t JOB_BYTE = 10;             // if open (E_TENT), job 
const uint16_t CHANNEL_BYTE = 11;         // if open (E_TENT), channel
const uint16_t CREATION_DATE_WORD = 12;   // creation date (packed word)
const uint16_t ENTRY_LENGTH = 14;         // length of entry with no extra bytes

const uint16_t FIRST_SEGMENT_SECTOR = 6;  // sector address of first sector of seg #1
const uint16_t SECTORS_PER_SEGMENT = 2;
}

namespace RT11FS {

DirScan::DirScan()
  : segment(-1)
  , segbase(0)
  , offset(0)
  , datasec(0)
{
}

auto operator ==(const Rad50Name &left, const Rad50Name &right) 
{
  return 
    left.at(0) == right.at(0) && 
    left.at(1) == right.at(1) &&
    left.at(2) == right.at(2);
}

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

Directory::~Directory()
{
  if (dirblk) {
    cache->putBlock(dirblk);
  }
}

auto Directory::getEnt(const std::string &name, DirEnt &ent) -> int
{
  auto rad50Name = Rad50Name {};
  if (!parseFilename(name, rad50Name)) {
    return -EINVAL;
  }

  auto ds = DirScan {};

  while (moveNext(ds)) {
    auto status = dirblk->extractWord(ds.segbase + ds.offset + STATUS_WORD);
    if ((status & Dir::E_EOS) != 0) {
      continue;
    }

    if (
      rad50Name[0] == dirblk->extractWord(ds.segbase + ds.offset + FILENAME_WORDS) &&
      rad50Name[1] == dirblk->extractWord(ds.segbase + ds.offset + FILENAME_WORDS + 2) &&
      rad50Name[2] == dirblk->extractWord(ds.segbase + ds.offset + FILENAME_WORDS + 4)
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

auto Directory::getEnt(const DirScan &scan, DirEnt &ent) -> bool
{
  auto entoffs = scan.segbase + scan.offset;

  if (scan.afterEnd()) {
    return false;
  }

  ent.name = Rad50::fromRad50(dirblk->extractWord(entoffs + FILENAME_WORDS));
  ent.name += Rad50::fromRad50(dirblk->extractWord(entoffs + FILENAME_WORDS + 2));

  ent.name = rtrim(ent.name);
  ent.name += ".";
  
  ent.name += Rad50::fromRad50(dirblk->extractWord(entoffs + FILENAME_WORDS + 4));
  ent.name = rtrim(ent.name);

  ent.status = dirblk->extractWord(entoffs + STATUS_WORD);
  ent.length = dirblk->extractWord(entoffs + TOTAL_LENGTH_WORD) * Block::SECTOR_SIZE;
  ent.sector0 = scan.datasec;

  struct tm tm;
  memset(&tm, 0, sizeof(tm));

  auto dateWord = dirblk->extractWord(entoffs + CREATION_DATE_WORD); 

  auto age = (dateWord >> 14) & 03;  
  tm.tm_mon = ((dateWord >> 10) & 017) - 1;
  tm.tm_mday = (dateWord >> 5) & 037;
  tm.tm_year = 72 + age * 32 + (dateWord & 037);

  ent.create_time = mktime(&tm);  

  return true;
}

auto Directory::startScan() -> DirScan
{
  return DirScan {};
}

auto Directory::moveNext(DirScan &scan) -> bool
{
  if (scan.afterEnd()) {
    return false;
  }

  if (scan.beforeStart()) {
    scan.segment = 1;
    scan.segbase = (scan.segment - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
    scan.offset = FIRST_ENTRY_OFFSET;
    scan.datasec = dirblk->extractWord(scan.segbase + SEGMENT_DATA_BLOCK);

    return true;
  }

  auto extra = dirblk->extractWord(EXTRA_BYTES);
  auto status = dirblk->extractWord(scan.segbase + scan.offset + STATUS_WORD);

  // if it's not an end of segment marker
  if ((status & Dir::E_EOS) == 0) {
    scan.datasec += dirblk->extractWord(
      scan.segbase + scan.offset + TOTAL_LENGTH_WORD);
    scan.offset += ENTRY_LENGTH + extra;
    return true;
  }

  // we're done if there's no next segment
  scan.segment = dirblk->extractWord(scan.segbase + NEXT_SEGMENT);
  if (scan.afterEnd()) {
    return false;
  }

  // set up at start of next segment 
  scan.segbase = (scan.segment - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
  scan.offset = FIRST_ENTRY_OFFSET;
  scan.datasec = dirblk->extractWord(scan.segbase + SEGMENT_DATA_BLOCK);

  return true;
}

auto Directory::moveNextFiltered(DirScan &scan, uint16_t mask) -> bool
{
  while (moveNext(scan)) {
    auto status = dirblk->extractWord(scan.segbase + scan.offset + STATUS_WORD);
    if ((status & mask) != 0) {
      return true;
    }
  }

  return false;
}

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

  auto scan = startScan();
  auto ent = DirEnt {};
    
  auto usedinodes = 0;
  auto usedblocks = 0;
  auto freeblocks = 0;

  while (moveNext(scan)) {
    getEnt(scan, ent);
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