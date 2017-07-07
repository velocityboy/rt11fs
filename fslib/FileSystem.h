#ifndef __FILESYSTEM_H_
#define __FILESYSTEM_H_

#include <functional>
#include <fuse.h>
#include <memory>
#include <string>
#include <vector>

namespace RT11FS {

class BlockCache;
class DataSource;
class Directory;
struct DirEnt;
class File;
class OpenFileTable;

class FileSystem
{
public:
  FileSystem(const std::string &name);
  ~FileSystem();

  auto getDirectory() { return directory.get(); }

  auto getattr(const char *path, struct stat *stbuf) -> int;
  auto fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) -> int;
  auto statfs(const char *, struct statvfs *fs) -> int;
  auto chmod(const char *, mode_t) -> int;
  auto rename(const char *, const char *) -> int;

  auto readdir(
    const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) -> int;
  auto open(const char *path, struct fuse_file_info *fi) -> int;
  auto release(const char *path, struct fuse_file_info *fi) -> int;
  auto read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi) -> int;
  auto write(const char *path, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi) -> int;

  auto ftruncate(const char *path, off_t size, struct fuse_file_info *fi) -> int;
  auto fsync(const char *path, int isdatasync, struct fuse_file_info *fi) -> int;

  // utilities which aren't properly part of the file system
  auto lsdir() -> void;

private:
  int fd;
  std::unique_ptr<DataSource> dataSource;
  std::unique_ptr<BlockCache> cache;
  std::unique_ptr<Directory> directory;
  std::unique_ptr<OpenFileTable> oft;

  static auto wrapper(std::function<int(void)> fn) -> int;
  auto validatePath(std::string &path) -> int;
}; 

};

#endif
