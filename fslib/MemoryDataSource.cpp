#include "MemoryDataSource.h"

#include <cerrno>
#include <cstring>

namespace RT11FS {

MemoryDataSource::MemoryDataSource(size_t bytes)
{
  memory.resize(bytes);
}

auto MemoryDataSource::stat(struct stat *st) -> int
{
  memset(st, 0, sizeof(struct stat));

  // all the interface really cares about is the file size
  st->st_size = memory.size();
  return 0;
}

auto MemoryDataSource::read(void *buffer, size_t bytes, off_t offset) -> ssize_t 
{
  if (
    offset < 0 ||                       // NOTE off_t is signed
    offset + bytes > memory.size()
  ) {
    return -EIO;
  }

  ::memcpy(buffer, &memory[offset], bytes);
  return bytes;
}

auto MemoryDataSource::write(void *buffer, size_t bytes, off_t offset) -> ssize_t
{
  if (
    offset < 0 ||                       // NOTE off_t is signed
    offset + bytes > memory.size()
  ) {
    return -EIO;
  }

  ::memcpy(&memory[offset], buffer, bytes);
  return bytes;
}
  
}