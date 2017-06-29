#include "BlockCache.h"
#include "Directory.h"
#include "File.h"
#include "FileSystem.h"
#include "FileSystemException.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using std::cerr;
using std::endl;
using std::find_if;
using std::make_unique;
using std::string;

namespace RT11FS {

FileSystem::FileSystem(const string &name)
  : fd(-1)
{
  fd = ::open(name.c_str(), O_RDWR|O_EXLOCK);
  if (fd == -1) {
    throw FilesystemException {-ENOENT, "volume file could not be opened"};
  }

  cache = make_unique<BlockCache>(fd);
  directory = make_unique<Directory>(cache.get());
}

FileSystem::~FileSystem()
{
  if (fd == -1) {
    close(fd);
  }
}

auto FileSystem::getattr(const char *path, struct stat *stbuf) -> int
{
  return wrapper([this, path, stbuf]() {
    memset(stbuf, 0, sizeof(struct stat));
    auto p = string {path};

    if (p == "/") {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 3;
      return 0;
    }

    auto ent = DirEnt {};
    auto err = getDirEnt(p, ent);

    if (err == 0) {
      uint16_t perm = 0444;
      if ((ent.status & Dir::E_READ) == 0) {
        perm |= 0222;
      }

      stbuf->st_mode = S_IFREG | perm;
      stbuf->st_nlink = 1;
      stbuf->st_size = ent.length;
      stbuf->st_mtime = ent.create_time;
    }

    return err;
  });
}

auto FileSystem::statfs(const char *path, struct statvfs *vfs) -> int
{
  return wrapper([this, path, vfs]() {
    auto p = string {path};

    if (p != "/") {
      return -ENOENT;
    }

    return directory->statfs(vfs);
  });
}


auto FileSystem::readdir(
  const char *path, void *buf, fuse_fill_dir_t filler,
  off_t offset, struct fuse_file_info *fi) -> int
{
  return wrapper([this, path, buf, filler, offset, fi]() {
    auto p = string {path};
    if (p != "/") {
      return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    auto scan = directory->startScan();
    while (directory->moveNextFiltered(scan, Dir::E_PERM)) {
      auto ent = DirEnt {};
      if (directory->getEnt(scan, ent)) {
        filler(buf, ent.name.c_str(), NULL, 0);
      }
    }

    return 0;
  });
}

auto FileSystem::open(const char *path, struct fuse_file_info *fi) -> int
{
  return wrapper([this, path, fi]() {
    auto ent = DirEnt {};
    auto err = getDirEnt(path, ent);
    if (err < 0) {
      return err;
    }

    auto fh = getEmptyFileSlot();
    files[fh] = move(make_unique<File>(cache.get(), ent));

    fi->fh = fh;

    return 0;
  });
}

auto FileSystem::release(const char *path, struct fuse_file_info *fi) -> int
{
  return wrapper([this, fi]() {
    getHandle(fi->fh);
    files[fi->fh].reset();
    return 0;
  });
}


auto FileSystem::read(
  const char *path, char *buf, size_t count, off_t offset, 
  struct fuse_file_info *fi) -> int 
{
  return wrapper([this, path, buf, count, offset, fi] {
    return getHandle(fi->fh)->read(buf, count, offset);
  });
}

auto FileSystem::ftruncate(const char *path, off_t size, struct fuse_file_info *fi) -> int
{
  return wrapper([this, size, fi]() {
    return directory->truncate(
      getHandle(fi->fh)->getDirEnt(),
      size);
  });
}

auto FileSystem::wrapper(std::function<int(void)> fn) -> int
{
  auto err = 0;

  try {
    err = fn();
  } catch (FilesystemException fse) {
    cerr << fse.what() << endl;
    return fse.getError();
  } catch (std::bad_alloc) {
    cerr << "out of memory" << endl;
    return -ENOMEM;
  } catch (...) {
    cerr << "unexpected exception" << endl;
    return -EINVAL;
  }

  return err;
}

auto FileSystem::getDirEnt(const string &path, DirEnt &de) -> int
{
  if (path == "" || path[0] != '/') {
    return -EINVAL;
  }

  if (path == "/") {
    return -ENOENT;
  }

  if (path.find('/', 1) != string::npos) {
    return -ENOENT;    
  }

  return directory->getEnt(path.substr(1), de);
}

auto FileSystem::getEmptyFileSlot() -> int
{
  auto fileIter = find_if(begin(files), end(files), [](const auto &slot) { 
    return slot == nullptr; 
  });

  if (fileIter != end(files)) {
    return fileIter - begin(files);
  }

  files.push_back(nullptr);
  return files.size() - 1;
}

auto FileSystem::getHandle(int fh) -> File *
{
  if (fh < 0 || fh >= files.size() || files[fh] == nullptr) {
    throw FilesystemException {-EBADF, "Invalid file handle"};
  }
  return files[fh].get();
}

}