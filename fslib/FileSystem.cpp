#include "BlockCache.h"
#include "Directory.h"
#include "FileDataSource.h"
#include "FileSystem.h"
#include "FileSystemException.h"
#include "OpenFileTable.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using std::cout;
using std::cerr;
using std::endl;
using std::find_if;
using std::make_unique;
using std::setfill;
using std::setw;
using std::string;
using std::vector;

namespace RT11FS {

FileSystem::FileSystem(const string &name)
  : fd(-1)
{
  fd = ::open(name.c_str(), O_RDWR|O_EXLOCK);
  if (fd == -1) {
    throw FilesystemException {-ENOENT, "volume file could not be opened"};
  }

  dataSource = make_unique<FileDataSource>(fd);

  cache = make_unique<BlockCache>(dataSource.get());
  directory = make_unique<Directory>(cache.get());
  oft = make_unique<OpenFileTable>(directory.get(), cache.get());
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
      stbuf->st_mode = S_IFDIR | 0777;
      stbuf->st_nlink = 3;
      return 0;
    }

    auto parsedPath = string {path};
    auto err = validatePath(parsedPath);
    if (err < 0) {
      return err;
    }

    auto ent = DirEnt {};
    err = directory->getEnt(parsedPath, ent);
    if (err < 0) {
      return err;
    }

    uint16_t perm = 0444;
    if ((ent.status & Dir::E_READ) == 0) {
      perm |= 0222;
    }

    stbuf->st_mode = S_IFREG | perm;
    stbuf->st_nlink = 1;
    stbuf->st_size = ent.length;
    stbuf->st_mtime = ent.create_time;

    return 0;
  });
}

auto FileSystem::fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) -> int
{
  return getattr(path, stbuf);
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

auto FileSystem::chmod(const char *, mode_t) -> int
{
  // TODO RT-11 doesn't support permissions, but we could set the R/O flag
  return 0;
}

auto FileSystem::unlink(const char *path) -> int
{
  return wrapper([this, path](){
    auto parsedPath = string {path};
    auto err = validatePath(parsedPath);
    if (err < 0) {
      return err;
    }

    return oft->unlink(parsedPath);
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
    auto parsedPath = string {path};
    auto err = validatePath(parsedPath);
    if (err < 0) {
      return err;
    }

    auto fd = oft->openFile(parsedPath);
    if (fd < 0) {
      return fd;
    }

    fi->fh = fd;

    return 0;
  });
}

auto FileSystem::create(const char *path, mode_t mode, struct fuse_file_info *fi) -> int
{
  return wrapper([this, path, mode, fi](){
    auto parsedPath = string {path};
    auto err = validatePath(parsedPath);
    if (err < 0) {
      return err;
    }

    if ((mode & S_IFMT) != S_IFREG) {
      return -EINVAL;
    }

    auto fd = oft->createFile(parsedPath);
    if (fd < 0) {
      return fd;
    }

    fi->fh = fd;

    return 0;
  });
}


auto FileSystem::release(const char *path, struct fuse_file_info *fi) -> int
{
  return wrapper([this, fi]() {
    return oft->closeFile(fi->fh);
  });
}


auto FileSystem::read(
  const char *path, char *buf, size_t count, off_t offset, 
  struct fuse_file_info *fi) -> int 
{
  return wrapper([this, path, buf, count, offset, fi] {
    return oft->readFile(fi->fh, buf, count, offset);
  });
}

auto FileSystem::write(
  const char *path, const char *buf, size_t count, off_t offset,
  struct fuse_file_info *fi) -> int
{
  return wrapper([this, path, buf, count, offset, fi] {
    return oft->writeFile(fi->fh, buf, count, offset);
  });
}

auto FileSystem::ftruncate(const char *path, off_t size, struct fuse_file_info *fi) -> int
{
  return wrapper([this, size, fi]() {    
    return oft->truncate(fi->fh, size);
  });
}

auto FileSystem::fsync(const char *path, int isdatasync, struct fuse_file_info *fi) -> int
{
  return wrapper([this] {
    cache->sync();
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

auto FileSystem::validatePath(string &path) -> int
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

  path = path.substr(1);
  return 0;
}

auto FileSystem::lsdir() -> void
{
  auto dirp = directory->startScan();

  cout << "SEG,IDX ---NAME--- LENGTH SECTOR" << endl;
  while (++dirp) {
    auto status = dirp.getWord(Dir::STATUS_WORD);

    auto ent = DirEnt {};

    directory->getEnt(dirp, ent);
    cout << setfill(' ');
    cout << setw(3) << dirp.getSegment() << "," << setw(3) << dirp.getIndex() << " ";
    if (dirp.hasStatus(Dir::E_MPTY)) {
      cout << setw(10) << "<FREE>";
    } else {
      cout << setw(10) << ent.name;
    }
    cout << " " << setw(6) << ent.length / Block::SECTOR_SIZE;
    cout << " " << setw(6) << ent.sector0;

    auto date = dirp.getWord(Dir::CREATION_DATE_WORD);
    if (date == 0) {
      cout << "     -  -  ";
    } else {
      auto age = (date >> 14) & 0x03;
      auto month = (date >> 9) & 0x0f;
      auto day = (date >> 4) & 0x1f;
      auto year = date & 0x1f;

      year += 1972 + 32 * age;

      cout << " " 
        << setw(4) << year
        << "-"
        << setw(2) << setfill('0') << month - 1
        << "-"
        << setw(2) << setfill('0') << day;
    }

    cout << " ";
    cout << ((status & Dir::E_TENT) ? "TEN" : "   ");
    cout << " ";
    cout << ((status & Dir::E_MPTY) ? "MPT" : "   ");
    cout << " ";
    cout << ((status & Dir::E_PERM) ? "PRM" : "   ");
    cout << " ";
    cout << ((status & Dir::E_EOS) ? "EOS" : "  ");
    cout << " ";
    cout << ((status & Dir::E_READ) ? "RDO" : "   ");
    cout << " ";
    cout << ((status & Dir::E_PROT) ? "PRT" : "   ");
    cout << " ";
    cout << ((status & Dir::E_PRE) ? "PRE" : "   ");

    cout << endl;
  }
}


}