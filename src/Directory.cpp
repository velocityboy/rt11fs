#include "BlockCache.h"
#include "Directory.h"
#include "FilesystemException.h"
#include "Rad50.h"

#include <cerrno>


using std::string;


namespace RT11FS {
using namespace Dir;

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

  auto ds = startScan();

  while (moveNext(ds)) {
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

auto Directory::startScan() -> DirPtr
{
  return DirPtr {dirblk};
}

auto Directory::moveNext(DirPtr &ptr) -> bool
{
  ptr++;
  return !ptr.afterEnd();
}

auto Directory::moveNextFiltered(DirPtr &ptr, uint16_t mask) -> bool
{
  while (moveNext(ptr)) {
    if ((ptr.getWord(STATUS_WORD) & mask) != 0) {
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