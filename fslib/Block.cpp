#include "Block.h"
#include "DataSource.h"
#include "FilesystemException.h"

#include <cerrno>
#include <unistd.h>

using std::exception;

namespace RT11FS {

Block::Block(int sector, int count)
  : sector(sector)
  , count(count)
  , dirty(false)
  , refcount(0)
{
  data.resize(count * SECTOR_SIZE);
}

// Extract a word from the block in PDP-11 byte order
auto Block::extractWord(int offset) -> uint16_t
{
  return data.at(offset) | (data.at(offset + 1) << 8);
}

// set a byte
auto Block::setByte(int offset, uint8_t value) -> void
{
  data.at(offset) = value;
  dirty = true;
}

// set a word
auto Block::setWord(int offset, uint16_t value) -> void
{
  data.at(offset) = value & 0377;
  data.at(offset + 1) = (value >> 8) & 0377;
  dirty = true;
}

// Read the contents of the file system image into the block.
auto Block::read(DataSource *dataSource) -> void
{
  auto toSeek = sector * SECTOR_SIZE;
  auto toRead = count * SECTOR_SIZE;

  // Data source is defined to return an error if the entire read cannot 
  // be satisfied
  int err = dataSource->read(&data[0], toRead, toSeek);
  if (err < 0) {
    throw FilesystemException {err, "could not read block"};
  }

  dirty = false;
}

// Copy some data into an external buffer. The caller is
// responsible for ensuring that all the data can be 
// provided from this block.
auto Block::copyOut(int offset, int bytes, char *dest) -> void
{
  if (offset + bytes > data.size()) {
    throw FilesystemException {-EIO, "read past end of block"};
  }

  memcpy(dest, &data[offset], bytes);
}

// Copy data around within the block. Safe even if the ranges overlap.
auto Block::copyWithinBlock(int sourceOffset, int destOffset, int count) -> void
{
  if (
    // all parameters must be positive
    sourceOffset < 0 || 
    destOffset < 0 || 
    count <= 0 || 
    // these check for integer overflow
    sourceOffset + count <= 0 ||
    destOffset + count <= 0 ||
    // can't run off end of block
    sourceOffset + count > data.size() ||
    destOffset + count > data.size()) {
    throw FilesystemException {-EIO, "invalid copy ranges for moving data inside block"};    
  }

  ::memmove(
    &data[destOffset],
    &data[sourceOffset],
    count);

  dirty = true;  
}

/**
 * Copy data from one block to another
 * 
 * Violating the bounds of either block will cause -EIO to be thrown
 *
 * @param source the block to copy data from.
 * @param sourceOffset the byte offset to copy from in `source'.
 * @param destOffset the byte offset to copy to in this block.
 * @param count the number of bytes to copy.
 */
auto Block::copyFromOtherBlock(Block *source, int sourceOffset, int destOffset, int count) -> void
{
  if (
    // all parameters must be positive
    sourceOffset < 0 || 
    destOffset < 0 || 
    count <= 0 || 
    // these check for integer overflow
    sourceOffset + count <= 0 ||
    destOffset + count <= 0 ||
    // can't run off end of block
    sourceOffset + count > source->data.size() ||
    destOffset + count > data.size()) {
    throw FilesystemException {-EIO, "invalid copy ranges for moving data between blocks"};    
  }

  // It's safe to use memcpy here because two blocks can never overlap
  ::memcpy(
    &data[destOffset],
    &source->data[sourceOffset],
    count);

  dirty = true;
}

// Resize the block to a new number of sectors. If the block is
// growing, then fill the new space from the file system image.
auto Block::resize(int newCount, DataSource *dataSource) -> void
{
  data.resize(newCount * SECTOR_SIZE);  

  if (newCount > count) {
    auto toSeek = (sector + count) * SECTOR_SIZE;
    auto toRead = (newCount - count) * SECTOR_SIZE;
    auto at = count * SECTOR_SIZE;

    try {
      int err = dataSource->read(&data[at], toRead, toSeek);
      if (err < 0) {
        throw FilesystemException {err, "could not read block"};
      }
    } catch (exception) {
      data.resize(count);
      throw;
    }
  }
}

}
