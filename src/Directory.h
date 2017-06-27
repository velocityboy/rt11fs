#ifndef __DIRECTORY_H_
#define __DIRECTORY_H_

#include <array>
#include <cstdint>
#include <ctime>
#include <fuse.h>
#include <string>

namespace RT11FS {

namespace Dir
{
const uint16_t E_TENT = 0000400;        // entry is tentative (open)
const uint16_t E_MPTY = 0001000;        // entry is free space
const uint16_t E_PERM = 0002000;        // entry is permanent (a real file)
const uint16_t E_EOS  = 0004000;        // entry marks end of segment
const uint16_t E_READ = 0040000;        // entry is read-only file
const uint16_t E_PROT = 0100000;        // entry is protected
const uint16_t E_PRE  = 0000020;        // entry has prefix blocks
}

class Block;
class BlockCache;

using Rad50Name = std::array<uint16_t, 3>;

struct DirEnt {
  uint16_t status;
  Rad50Name rad50_name;
  std::string name;
  int length;
  int sector0;
  time_t create_time;
};

class DirScan 
{
public:
  DirScan();

  auto beforeStart() const { return segment == -1; }
  auto afterEnd() const { return segment == 0; }

  int segment;
  int segbase;
  int offset;
  int datasec;  
};

class Directory
{
public:
  Directory(BlockCache *cache);
  ~Directory();

  auto getEnt(const std::string &name, DirEnt &ent) -> int;
  auto getEnt(const DirScan &scan, DirEnt &ent) -> bool;
  auto startScan() -> DirScan;
  auto moveNext(DirScan &scan) -> bool;
  auto moveNextFiltered(DirScan &scan, uint16_t mask) -> bool;
  auto statfs(struct statvfs *vfs) -> int;

private:
  BlockCache *cache;
  Block *dirblk;

  static auto parseFilename(const std::string &name, Rad50Name &rad50) -> bool;
};
}

#endif
