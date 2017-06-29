#ifndef __DIRECTORY_H_
#define __DIRECTORY_H_

#include "DirConst.h"
#include "DirPtr.h"

#include <array>
#include <cstdint>
#include <ctime>
#include <fuse.h>
#include <string>

namespace RT11FS {

class Block;
class BlockCache;

struct DirEnt {
  uint16_t status;
  Dir::Rad50Name rad50_name;
  std::string name;
  int length;
  int sector0;
  time_t create_time;
};

class Directory
{
public:
  Directory(BlockCache *cache);
  ~Directory();

  auto getEnt(const std::string &name, DirEnt &ent) -> int;
  auto getEnt(const DirPtr &ptr, DirEnt &ent) -> bool;
  auto startScan() -> DirPtr;
  auto moveNext(DirPtr &ptr) -> bool;
  auto moveNextFiltered(DirPtr &ptr, uint16_t mask) -> bool;
  auto statfs(struct statvfs *vfs) -> int;

private:
  BlockCache *cache;
  Block *dirblk;

  static auto parseFilename(const std::string &name, Dir::Rad50Name &rad50) -> bool;
};
}

#endif
