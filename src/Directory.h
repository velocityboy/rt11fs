#ifndef __DIRECTORY_H_
#define __DIRECTORY_H_

#include "DirConst.h"
#include "DirPtr.h"

#include <array>
#include <cstdint>
#include <ctime>
#include <fuse.h>
#include <string>
#include <vector>

namespace RT11FS {

class Block;
class BlockCache;

struct DirEnt {
  uint16_t status;              /*!< The status word of the entry */
  Dir::Rad50Name rad50_name;    /*!< The file name encoded as Rad50 */
  std::string name;             /*!< The file name as a printable string */
  int length;                   /*!< The length of the file in bytes */
  int sector0;                  /*!< The first data of the file */
  time_t create_time;           /*!< The creation time of the file */
};

class Directory
{
public:
  Directory(BlockCache *cache);
  ~Directory();

  auto getEnt(const std::string &name, DirEnt &ent) -> int;
  auto getDirPointer(const Dir::Rad50Name &name) -> DirPtr;
  auto getEnt(const DirPtr &ptr, DirEnt &ent) -> bool;
  auto startScan() -> DirPtr;
  auto moveNextFiltered(DirPtr &ptr, uint16_t mask) -> bool;
  auto statfs(struct statvfs *vfs) -> int;
  auto truncate(const DirEnt &ent, off_t size) -> int;

private:
  int entrySize;
  BlockCache *cache;
  Block *dirblk;

#if TRUNCATE_CODE
  auto findEnt(const DirEnt &ent) -> DirScan;
  auto shrinkEntry(const DirScan &ds, int newSize) -> int;
  auto insertEmptyAt(const DirScan &ds) -> bool;
  auto maxEntriesPerSegment() -> int;
  auto isSegmentFull(int segmentIndex) -> bool;
  auto lastSegmentEntry(int segmentIndex) -> int;
  auto offsetOfEntry(int segment, int index) -> int;
  auto firstOfSegment(int segment) -> DirScan;
#endif
  static auto parseFilename(const std::string &name, Dir::Rad50Name &rad50) -> bool;
};
}

#endif
