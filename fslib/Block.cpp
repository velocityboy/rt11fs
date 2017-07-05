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

/**
 * Get a byte from the block.
 *
 * Throws std::out_of_range if offset is invalid.
 *
 * @param offset is the offset of the byte to retrieve.
 * @return the value of the byte at `offset'.
 */
auto Block::getByte(int offset) -> uint8_t
{
  return data.at(offset);
}

/**
 * Get a word from the block.
 *
 * Throws std::out_of_range if offset is invalid. The word will
 * be read in PDP-11 byte order.
 *
 * @param offset is the offset of the word to retrieve.
 * @return the value of the word at `offset'.
 */
auto Block::extractWord(int offset) -> uint16_t
{
  return data.at(offset) | (data.at(offset + 1) << 8);
}

/**
 * Set a byte in the block.
 *
 * Throws std::out_of_range if offset is invalid. 
 *
 * @param offset is the offset of the byte to store.
 * @parm value the byte to store at `offset'.
 */
auto Block::setByte(int offset, uint8_t value) -> void
{
  data.at(offset) = value;
  dirty = true;
}

/**
 * Set a word in the block.
 *
 * Throws std::out_of_range if offset is invalid. 
 * The word will be stored in PDP-11 byte order.
 *
 * @param offset is the offset of the word to store.
 * @parm value the word to store at `offset'.
 */
auto Block::setWord(int offset, uint16_t value) -> void
{
  data.at(offset) = value & 0377;
  data.at(offset + 1) = (value >> 8) & 0377;
  dirty = true;
}

/**
 * Read data from the mounted data source into the block.
 *
 * Throws an exception on I/O errors.
 * It is the responsibility of the caller to ensure that 
 * the block is first written if it is dirty.
 *
 * @param dataSource the interface to the underlying mounted data source.
 */
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

/**
 * Write data from the block into the mounted data source.
 *
 * Throws an exception on I/O errors. 
 *
 * @param dataSource the interface to the underlying mounted data source.
 */
auto Block::write(DataSource *dataSource) -> void
{
  auto toSeek = sector * SECTOR_SIZE;
  auto toWrite = count * SECTOR_SIZE;

  // Data source is defined to return an error if the entire write cannot 
  // be satisfied
  int err = dataSource->write(&data[0], toWrite, toSeek);
  if (err < 0) {
    throw FilesystemException {err, "could not read block"};
  }

  dirty = false;
}

/**
 * Copy data out of the block to a caller buffer.
 * 
 * Throws an -EIO exception if the requested copy exceeds the bounds of 
 * the block.
 *
 * @param offset the offset in the block to copy from.
 * @param bytes the number of bytes to copy.
 * @param dest the buffer to copy the data into.
 */
auto Block::copyOut(int offset, int bytes, char *dest) -> void
{
  if (offset + bytes > data.size()) {
    throw FilesystemException {-EIO, "read past end of block"};
  }

  memcpy(dest, &data[offset], bytes);
}

/**
 * Copy data into the block from a caller buffer.
 * 
 * Throws an -EIO exception if the requested copy exceeds the bounds of 
 * the block.
 *
 * @param offset the offset in the block to copy to.
 * @param bytes the number of bytes to copy.
 * @param dest the buffer to copy the data from.
 */
auto Block::copyIn(int offset, int bytes, const char *src) -> void 
{
  if (offset + bytes > data.size()) {
    throw FilesystemException {-EIO, "write past end of block"};
  }

  memcpy(&data[offset], src, bytes);
  
  dirty = true;  
}

/**
 * Copy data inside the block.
 * 
 * Throws an -EIO exception if the requested copy exceeds the bounds of 
 * the block.
 * 
 * It is safe to copy a source range that overlaps the destination range.
 *
 * @param sourceOffset the offset in the block to copy from.
 * @param destOffset the offset in the block to copy to.
 * @param count the number of bytes to copy.
 */
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

/**
 * Zero fill part of a block
 *
 * Violating the bounds of the block will cause -EIO to be thrown
 *
 * @param offset the offset to start filling at.
 * @param count the number of bytes to fill with zeroes.
 */
auto Block::zeroFill(int offset, int count) -> void
{
  if (
    offset < 0 ||
    offset + count <= 0 ||
    offset + count > data.size()) {
    throw FilesystemException {-EIO, "Invalid range for zero filling blocks"};
  }

  ::memset(&data[offset], 0, count);

  dirty = true;
}

/**
 * Resize the block.
 *
 * The block may either grow or shrink. If it grows, new data will be read
 * from the data source to fill in the additional size. If that I/O results
 * in an error, an exception will be thrown.
 *
 * The expected use of this call is to expand a block to contain an entire
 * data structure (the disk directory) after the first part, from which the
 * total size can be determined, has been read.
 * 
 * @param newCount the new size of the block in sectors.
 * @param dataSource the data source to use to backfill the data if the block is growing.
 */
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

  count = newCount;
}

}
