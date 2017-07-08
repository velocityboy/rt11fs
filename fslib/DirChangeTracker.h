// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __DIRCHANGETRACKER_H_
#define __DIRCHANGETRACKER_H_

#include "DirChange.h"
#include "DirPtr.h"
#include <vector>

/**
 * Tracks entries as they move around a directory.
 *
 * Changes are added to the tracker in transactions. Transactions are atomic, so if
 * 1:1 moves to 1:2 and 1:2 moves to 1:3 in the same transation, they will be recorded 
 * as two entries. However, if 1:1 moves to 1:2 in one transaction, and 1:2 moves to 1:3
 * in a later transaction, then 1:1 will be tracked as moving to 1:3. This is needed to 
 * support block moves where a portion of a directory segment is moved at once.
 */
class DirChangeTracker
{
public:
  struct Entry {
    int oldSegment;
    int oldIndex;
    int moveTransaction;
    int newSegment;
    int newIndex;
  };

  DirChangeTracker();
  auto beginTransaction() -> void;
  auto moveDirEntry(const RT11FS::DirPtr &src, const RT11FS::DirPtr &dst) -> void;
  auto endTransaction() -> void;
  auto dump() -> void;
  auto getMoves() -> std::vector<Entry> { return moves; }

private:
  int transaction;
  bool inTransaction;
  std::vector<Entry> moves;
};

#endif
