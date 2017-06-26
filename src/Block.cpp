#include "Block.h"
#include "FilesystemException.h"

#include <cerrno>
#include <unistd.h>

using std::exception;

namespace RT11FS {

Block::Block(int sector, int count)
  : sector(sector)
  , count(count)
{
  data.resize(count * SECTOR_SIZE);
}

// Extract a word from the block in PDP-11 byte order
auto Block::extractWord(int offset) -> uint16_t
{
  return data.at(offset) | (data.at(offset + 1) << 8);
}

// Read the contents of the file system image into the block.
auto Block::read(int fd) -> void
{
  auto toSeek = sector * SECTOR_SIZE;
  auto toRead = count * SECTOR_SIZE;

  if (lseek(fd, toSeek, SEEK_SET) != toSeek) {
    throw FilesystemException {-EIO, "could not seek to block"};
  }

  if (::read(fd, &data[0], toRead) != toRead) {
    throw FilesystemException {-EIO, "could not read full block"};
  }
}

// Resize the block to a new number of sectors. If the block is
// growing, then fill the new space from the file system image.
auto Block::resize(int newCount, int fd) -> void
{
  data.resize(newCount * SECTOR_SIZE);  

  if (newCount > count) {
    auto toSeek = (sector + count) * SECTOR_SIZE;
    auto toRead = (newCount - count) * SECTOR_SIZE;
    auto at = count * SECTOR_SIZE;

    try {
      if (lseek(fd, toSeek, SEEK_SET) != toSeek) {
        throw FilesystemException {-EIO, "could not seek to block"};
      }

      if (::read(fd, &data[at], toRead) != toRead) {
        throw FilesystemException {-EIO, "could not read full block"};
      }
    } catch (exception) {
      data.resize(count);
      throw;
    }
  }
}

}
