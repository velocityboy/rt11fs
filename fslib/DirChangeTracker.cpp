// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#include "DirChangeTracker.h"
#include "DirConst.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace RT11FS;

using std::cerr;
using std::endl;
using std::find_if;

/** 
 * Construct a tracker
 */
DirChangeTracker::DirChangeTracker()
  : transaction(-1)
  , inTransaction(false)
{
}
 
/**
 * Begin a transaction. Transactions cannot nest.
 */
auto DirChangeTracker::beginTransaction() -> void
{
  assert(!inTransaction);
  ++transaction;
  inTransaction = true;
}

/**
 * Add an entry move to the current transaction.
 *
 * @param src the starting position of the entry.
 * @param dst the ending position of the entry.
 */
auto DirChangeTracker::moveDirEntry(const DirPtr &src, const DirPtr &dst) -> void
{
  assert(inTransaction);

  // we only care about files
  //
  if (!(src.hasStatus(Dir::E_TENT) || src.hasStatus(Dir::E_PERM))) {
    return;
  }

  // we're looking for an entry that has already been moved in a previous transaction,
  // that is now being moved again.
  auto iter = std::find_if(begin(moves), end(moves), [src, this](const auto &e) {
    return 
      e.newSegment == src.getSegment() && 
      e.newIndex == src.getIndex() && 
      e.moveTransaction != transaction;
  });

  if (iter != end(moves)) {
    iter->moveTransaction = transaction;
    iter->newSegment = dst.getSegment();
    iter->newIndex = dst.getIndex();
  } else {
    Entry entry {
      .oldSegment = src.getSegment(),
      .oldIndex = src.getIndex(),
      .moveTransaction = transaction,
      .newSegment = dst.getSegment(),
      .newIndex = dst.getIndex()
    };
    moves.push_back(entry);
  }
}

/**
 * Finish a transaction. If entries have moved and wound up back where they 
 * started, they will be filtered out before the transaction is finalized.
 */
auto DirChangeTracker::endTransaction() -> void
{
  assert(inTransaction);
  inTransaction = false;

  // there are times when an entry gets moved multiple times and lands where it
  // started.
  auto iter = begin(moves);
  while (iter != end(moves)) {
    if (iter->oldSegment == iter->newSegment && iter->oldIndex == iter->newIndex) {
      iter = moves.erase(iter);
    } else {
      ++iter;
    }
  }

}

/**
 * Debug call to see the contents of the tracker
 */
auto DirChangeTracker::dump() -> void
{
  for (const auto &move : moves) {
    cerr << move.oldSegment << ":" << move.oldIndex << " -> " << move.newSegment << ":" << move.newIndex << endl;
  }
}

