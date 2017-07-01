#ifndef __FILE_H_
#define __FILE_H_

#include "Directory.h"

namespace RT11FS {
class File
{
public:
  File(BlockCache *cache, const DirEnt &dirent);

  auto read(char *buffer, size_t count, off_t offset) -> int;
  auto getDirEnt() -> const DirEnt & { return dirent; }

private:
  BlockCache *cache;
  DirEnt dirent;
};
}

#endif
