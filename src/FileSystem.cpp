#include "BlockCache.h"
#include "Directory.h"
#include "FileSystem.h"
#include "FileSystemException.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using std::cerr;
using std::endl;
using std::make_unique;
using std::string;

namespace RT11FS {

FileSystem::FileSystem(const string &name)
  : fd(-1)
{
  fd = open(name.c_str(), O_RDWR|O_EXLOCK);
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

    if (p.find('/', 1) != string::npos) {
      return -ENOENT;
    }

    auto ent = DirEnt {};
    auto err = directory->getEnt(p.substr(1), ent);

    if (err == 0) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = ent.length;
    }

    return err;
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



}