#ifndef __DIRPTR_H_
#define __DIRPTR_H_

#include <cstdint>

namespace RT11FS {
class Block;

class DirPtr
{
public:
  DirPtr(Block *dirblk);

  auto beforeStart() const { return segment == -1; }
  auto afterEnd() const { return segment == 0; }
  auto offset(int delta = 0) const -> int;
  auto getDataSector() const { return datasec; }
  auto getWord(int offs) const -> uint16_t;

  auto operator++() -> DirPtr &;
  auto operator++(int) -> DirPtr;

  /**
   * Check if the pointer points to a valid entry
   */
  operator bool() const { return !beforeStart() && !afterEnd(); }

private:
  Block *dirblk;      /*!< Pointer to a block that contains the entire directory */
  int entrySize;      /*!< The size of a directory entry, including any extra bytes */
  int segment;        /*!< The one-based index of the segment containing the pointed to entry */
  int index;          /*!< The zero-based index of the entry within its containing segment */
  int segbase;        /*!< The offset of the current segment in the directory block */
  int datasec;        /*!< The first data block of the referenced file */

  auto increment() -> void;
};
}


#endif
