#ifndef __FILE_H_
#define __FILE_H_

#include "Directory.h"

namespace RT11FS {
class File
{
public:
  File(BlockCache *cache, Directory *dir, const DirEnt &dirent);

  auto read(char *buffer, size_t count, off_t offset) -> int;
  auto write(const char *buffer, size_t count, off_t offset) -> int;
  auto getDirEnt() -> const DirEnt & { return dirent; }

private:
  BlockCache *cache;
  Directory *dir;
  DirEnt dirent;
};
}

#endif
