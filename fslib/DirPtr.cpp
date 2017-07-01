#include "Block.h"
#include "DirConst.h"
#include "DirPtr.h"

#include <cassert>

namespace RT11FS {
using namespace Dir;

/**
 * Construct a directory pointer
 * @param dirblk the block containing the entire directory.
 */
DirPtr::DirPtr(Block *dirblk)
  : dirblk(dirblk)
  , entrySize(ENTRY_LENGTH + dirblk->extractWord(EXTRA_BYTES))
  , segment(-1)
  , index(0)
  , segbase(0)
  , datasec(dirblk->extractWord(Dir::SEGMENT_DATA_BLOCK))
{
}

/**
 * Compute the offset of a field in the referenced entry.
 *
 * Computes the offset of a field in the entry referenced by this pointer.
 * The returned offset is relative to the start of the entire directory.
 *
 * @param delta the offset into the entry.
 * @return the offset into the referenced entry
 */
auto DirPtr::offset(int delta) const -> int
{
  return segbase + FIRST_ENTRY_OFFSET + index * entrySize + delta;
}

/**
 * Return a word from the current entry.
 *
 * @param offs the offset into the entry.
 * @return the word value at the given offset
 */
auto DirPtr::getWord(int offs) const -> uint16_t
{
  return dirblk->extractWord(offset(offs));
}

/**
 * Sets a byte in the current entry.
 *
 * @param offs the offset into the entry.
 * @param v the value to store at the given offset
 */
auto DirPtr::setByte(int offs, uint8_t v) -> void
{
  dirblk->setByte(offset(offs), v);
}

/**
 * Sets a word in the current entry.
 *
 * @param offs the offset into the entry.
 * @param v the value to store at the given offset
 */
auto DirPtr::setWord(int offs, uint16_t v) -> void
{
  dirblk->setWord(offset(offs), v);
}

/**
 * Sets a word in the segment header of the referenced segment.
 *
 * @param offs the offset into the header.
 * @param v the value to store at the given offset
 */
auto DirPtr::setSegmentWord(int offset, uint16_t v) -> void
{
  dirblk->setWord(segbase + offset, v);
}

/** 
 * Test the status word for set bits.
 * 
 * All of the bits in `mask' must be set to pass the test.
 *
 * @param mask contains bit bits to check in the status word.
 * @return true if all the set bits im `mask' are also set in the status word
 */
auto DirPtr::hasStatus(uint16_t mask) const -> bool
{
  return (getWord(STATUS_WORD) & mask) == mask;
}

/**
 * Move the pointer to the next entry.
 *
 * @return the incremented entry
 */
auto DirPtr::operator++() -> DirPtr &
{
  increment();
  return *this;
}

/**
 * Move the pointer to the next entry.
 *
 * @return the original entry
 */
auto DirPtr::operator++(int) -> DirPtr
{
  DirPtr pre = *this;

  increment();
  return pre;
}

/**
 * Returns the next entry in the directory.
 * If the pointer is already past the end, does nothing.
 * @return a pointer to the next sequential entry in the directory
 */
auto DirPtr::next() const -> DirPtr
{
  auto nextp = *this;
  return ++nextp;
}

/**
 * Move the pointer to the previous entry.
 *
 * @return the decremented entry
 */
auto DirPtr::operator--() -> DirPtr &
{
  decrement();
  return *this;
}

/**
 * Move the pointer to the previous entry.
 *
 * @return the original entry
 */
auto DirPtr::operator--(int) -> DirPtr
{
  DirPtr pre = *this;

  decrement();
  return pre;
}

/**
 * Returns the previous entry in the directory.
 * If the pointer is already before the start, does nothing.
 * @return a pointer to the previous sequential entry in the directory
 */
auto DirPtr::prev() const -> DirPtr
{
  auto prevp = *this;
  return ++prevp;
}

/**
 * Move the pointer to the next entry.
 * If the pointer is already past the end, nothing will change.
 */
auto DirPtr::increment() -> void
{
  if (afterEnd()) {
    return;
  }

  if (beforeStart()) {
    setSegment(1);
    index = 0;
    datasec = dirblk->extractWord(segbase + SEGMENT_DATA_BLOCK);

    return;
  }

  auto extra = dirblk->extractWord(EXTRA_BYTES);
  auto status = getWord(STATUS_WORD);

  // if it's not an end of segment marker
  if ((status & E_EOS) == 0) {
    datasec += getWord(TOTAL_LENGTH_WORD);
    index++;
    return;
  }

  // we're done if there's no next segment
  segment = dirblk->extractWord(segbase + NEXT_SEGMENT);
  if (afterEnd()) {
    return;
  }

  // set up at start of next segment 
  segbase = (segment - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
  index = 0;
  datasec = dirblk->extractWord(segbase + SEGMENT_DATA_BLOCK);
}

/**
 * Move the pointer to the previous entry.
 * If the pointer is already before the start, nothing will change.
 */
auto DirPtr::decrement() -> void
{
  // can't back up any more
  if (beforeStart()) {
    return;
  }

  if (afterEnd()) {
    // find the last segment
    setSegment(1);

    while (true) {
      auto next = dirblk->extractWord(segbase + NEXT_SEGMENT);
      if (next == 0) {
        break;
      }
      setSegment(next);
    }

    // find the last entry
    index = 0;
    datasec = dirblk->extractWord(segbase + SEGMENT_DATA_BLOCK);

    while ((getWord(STATUS_WORD) & E_EOS) == 0) {
      increment();
    }

    return;
  }

  // we have a normal entry
  if (index) {
    index--;
    datasec -= getWord(TOTAL_LENGTH_WORD);
    return;
  }

  // we're at the start of a segment, so we have to find the end of the
  // previous segment
  if (segment == 1) {
    // we're at the start of the first segment, flag before start state
    //
    segment = -1;
    return;
  }

  auto curr = segment;
  setSegment(1);

  while (true) {
    auto next = dirblk->extractWord(segbase + NEXT_SEGMENT);
    assert(next != 0);

    if (next == curr) {
      break;
    }

    setSegment(next);
  }

  // find the last entry
  index = 0;
  datasec = dirblk->extractWord(segbase + SEGMENT_DATA_BLOCK);

  while ((getWord(STATUS_WORD) & E_EOS) == 0) {
    increment();
  }
}

auto DirPtr::setSegment(int seg) -> void
{
  segment = seg;
  segbase = (segment - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
}

}