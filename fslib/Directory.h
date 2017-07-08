#ifndef __DIRECTORY_H_
#define __DIRECTORY_H_

#include "DirChangeTracker.h"
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

/**
 * Represents a directory entry in a form usable by a client
 */
struct DirEnt {
  uint16_t status;              /*!< The status word of the entry */
  Dir::Rad50Name rad50_name;    /*!< The file name encoded as Rad50 */
  std::string name;             /*!< The file name as a printable string */
  int length;                   /*!< The length of the file in bytes */
  int sector0;                  /*!< The first data of the file */
  time_t create_time;           /*!< The creation time of the file */
};

/**
 * Represents the entire directory data structure of a mounted RT-11 volume
 */
class Directory
{
public:
  Directory(BlockCache *cache);
  ~Directory();

  auto getEnt(const std::string &name, DirEnt &ent) -> int;
  auto getDirPointer(const std::string &name, std::unique_ptr<DirPtr> &dirpp) -> int;
  auto getDirPointer(const Dir::Rad50Name &name) -> DirPtr;
  auto getEnt(const DirPtr &ptr, DirEnt &ent) -> bool;
  auto startScan() -> DirPtr;
  auto moveNextFiltered(DirPtr &ptr, uint16_t mask) -> bool;
  auto statfs(struct statvfs *vfs) -> int;
  auto truncate(DirPtr &dirp, off_t size, std::vector<DirChangeTracker::Entry> &moves) -> int;
  auto removeEntry(const std::string &name, std::vector<DirChangeTracker::Entry> &moves) -> int;
  auto rename(const std::string &oldName, const std::string &newName) -> int;
  auto createEntry(const std::string &name, std::unique_ptr<DirPtr> &dirpp, std::vector<DirChangeTracker::Entry> &moves) -> int;
  auto makeEntryPermanent(DirPtr &ptr) -> void;

private:
  int entrySize;
  BlockCache *cache;
  Block *dirblk;

  auto shrinkEntry(DirPtr &dirp, int newSize, DirChangeTracker &tracker) -> int;
  auto growEntry(DirPtr &dirp, int newSize, DirChangeTracker &tracker) -> int;

  auto insertEmptyAt(DirPtr &dirp, DirChangeTracker &tracker) -> int;
  auto deleteEmptyAt(DirPtr &dirp, DirChangeTracker &tracker) -> void;
  auto spillLastEntry(const DirPtr &dirp, DirChangeTracker &tracker) -> int;
  auto allocateNewSegment() -> int;
  auto findLargestFreeBlock() -> DirPtr;
  auto carveFreeBlock(DirPtr &dirp, int size, DirChangeTracker &tracker) -> int;
  auto coalesceNeighboringFreeBlocks(DirPtr &ptr, DirChangeTracker &tracker) -> void;

  auto maxEntriesPerSegment() const -> int;
  auto advanceToEndOfSegment(const DirPtr &dirp) -> DirPtr;

  auto moveEntriesWithinSegment(const DirPtr &src, const DirPtr &dst, int count, DirChangeTracker &tracker) -> void;
  auto moveEntryAcrossSegments(const DirPtr &src, const DirPtr &dst, DirChangeTracker &tracker) -> void;

  static auto parseFilename(const std::string &name, Dir::Rad50Name &rad50) -> bool;
  static auto dirTimeToTime(uint16_t dirTime, struct tm &tm) -> bool;
  static auto timeToDirTime(const struct tm &tm, uint16_t &dirtime) -> bool;
};
}

#endif
