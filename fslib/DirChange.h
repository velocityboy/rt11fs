// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __DIRCHANGE_H_
#define __DIRCHANGE_H_

namespace RT11FS {
/**
 * An interface for notifications about moved directory entries.
 *
 * Changes are transactionalized because otherwise, batches of changes could be ambigous.
 */
class DirChange {
public:
  virtual ~DirChange(){}

  virtual auto beginTransaction() -> void = 0;
  virtual auto moveDirEntry(int oldSegment, int oldIndex, int newSegment, int newIndex) -> void = 0;
  virtual auto endTransaction() -> void = 0;
};
}
#endif
