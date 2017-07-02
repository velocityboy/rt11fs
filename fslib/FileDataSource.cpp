#include "FileDataSource.h"

#include <cerrno>
#include <unistd.h>

namespace RT11FS {

FileDataSource::FileDataSource(int fd)
  : fd(fd)
{
}

FileDataSource::~FileDataSource()
{
  ::close(fd);
}


auto FileDataSource::stat(struct stat *st) -> int
{
  if (::fstat(fd, st) == -1) {
    return -errno;
  }

  return 0;
}

auto FileDataSource::read(void *buffer, size_t bytes, off_t offset) -> ssize_t
{
  auto seek = ::lseek(fd, offset, SEEK_SET);
  if (seek == -1) {
    return -errno;
  } else if (seek != offset) {
    return -EIO;
  }

  auto xfer = ::read(fd, buffer, bytes);
  if (xfer == -1) {
    return -errno;
  } else if (xfer != bytes) {
    return -EIO;
  }

  return xfer;
}

auto FileDataSource::write(void *buffer, size_t bytes, off_t offset) -> ssize_t
{
  auto seek = ::lseek(fd, offset, SEEK_SET);
  if (seek == -1) {
    return -errno;
  } else if (seek != offset) {
    return -EIO;
  }

  auto xfer = ::write(fd, buffer, bytes);
  if (xfer == -1) {
    return -errno;
  } else if (xfer != bytes) {
    return -EIO;
  }

  return xfer;
}


}