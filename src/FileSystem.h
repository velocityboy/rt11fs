#ifndef __FILESYSTEM_H_
#define __FILESYSTEM_H_

#include <functional>
#include <fuse.h>
#include <memory>
#include <string>
#include <vector>

namespace RT11FS {

class BlockCache;
class Directory;
struct DirEnt;
class File;

class FileSystem
{
public:
  FileSystem(const std::string &name);
  ~FileSystem();

  auto getDirectory() { return directory.get(); }

  auto getattr(const char *path, struct stat *stbuf) -> int;
  auto statfs(const char *, struct statvfs *fs) -> int;
  auto readdir(
    const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) -> int;
  auto open(const char *path, struct fuse_file_info *fi) -> int;
  auto release(const char *path, struct fuse_file_info *fi) -> int;
  auto read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi) -> int;
  auto ftruncate(const char *path, off_t size, struct fuse_file_info *fi) -> int;

private:
  int fd;  
  std::unique_ptr<BlockCache> cache;
  std::unique_ptr<Directory> directory;
  std::vector<std::unique_ptr<File>> files;

  static auto wrapper(std::function<int(void)> fn) -> int;
  auto getDirEnt(const std::string &path, DirEnt &de) -> int;
  auto getEmptyFileSlot() -> int;
  auto getHandle(int fh) -> File *;
}; 

};

#endif
