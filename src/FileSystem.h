#ifndef __FILESYSTEM_H_
#define __FILESYSTEM_H_

#include <functional>
#include <fuse.h>
#include <memory>
#include <string>

namespace RT11FS {

class BlockCache;
class Directory;

class FileSystem
{
public:
  FileSystem(const std::string &name);
  ~FileSystem();

  auto getDirectory() { return directory.get(); }

  auto getattr(const char *path, struct stat *stbuf) -> int;
  auto readdir(
    const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) -> int;

private:
  int fd;  
  std::unique_ptr<BlockCache> cache;
  std::unique_ptr<Directory> directory;

  static auto wrapper(std::function<int(void)> fn) -> int;
}; 

};

#endif
