#include "Block.h"
#include "DirConst.h"
#include "DirPtr.h"

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
 * @param delta the offset into the entry.
 * @return the word value at the given offset
 */
auto DirPtr::getWord(int offs) const -> uint16_t
{
  return dirblk->extractWord(offset(offs));
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
 * Move the pointer to the next entry.
 * If the pointer is already past the end, nothing will change.
 */
auto DirPtr::increment() -> void
{
  if (afterEnd()) {
    return;
  }

  if (beforeStart()) {
    segment = 1;
    segbase = (segment - 1) * SECTORS_PER_SEGMENT * Block::SECTOR_SIZE;
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

}