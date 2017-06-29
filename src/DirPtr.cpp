#include "Block.h"
#include "DirConst.h"
#include "DirPtr.h"

namespace RT11FS {
using namespace Dir;

DirPtr::DirPtr(Block *dirblk)
  : dirblk(dirblk)
  , entrySize(ENTRY_LENGTH + dirblk->extractWord(EXTRA_BYTES))
  , segment(-1)
  , index(0)
  , segbase(0)
  , datasec(dirblk->extractWord(Dir::SEGMENT_DATA_BLOCK))
{
}

auto DirPtr::offset(int delta) const -> int
{
  return segbase + FIRST_ENTRY_OFFSET + index * entrySize + delta;
}

auto DirPtr::operator++(int) -> DirPtr
{
  DirPtr pre = *this;

  increment();
  return pre;
}

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
  auto status = dirblk->extractWord(offset(STATUS_WORD));

  // if it's not an end of segment marker
  if ((status & E_EOS) == 0) {
    datasec += dirblk->extractWord(offset(TOTAL_LENGTH_WORD));
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