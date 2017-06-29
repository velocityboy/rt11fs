#ifndef __DIRPTR_H_
#define __DIRPTR_H_

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

  auto operator++(int) -> DirPtr;

private:
  Block *dirblk;
  int entrySize;
  int segment;
  int index;
  int segbase;
  int datasec;

  auto increment() -> void;
};
}


#endif
