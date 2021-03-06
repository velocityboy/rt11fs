// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __OPENFILETABLE_H_
#define __OPENFILETABLE_H_

#include "DirChangeTracker.h"
#include "DirPtr.h"

#include <string>
#include <vector>

namespace RT11FS {
class Directory;
class BlockCache;

/**
 * Tracks all open files in the system. Each entry in the open file table is one
 * file. The entries are refcounted so there may be multiple instances of the 
 * file being open; the table entry will be deleted when the last referencing
 * object closes.
 *
 * The OFT tracks file entries in the directory. The directory is responsible for 
 * updating the OFT when files move on disk.
 */
class OpenFileTable
{
public:
  OpenFileTable(Directory *dir, BlockCache *cache);

  auto openFile(const std::string &name) -> int;
  auto createFile(const std::string &name) -> int;
  auto closeFile(int fd) -> int;
  auto readFile(int fd, char *buffer, size_t count, off_t offset) -> int;
  auto writeFile(int fd, const char *buffer, size_t count, off_t offset) -> int;
  auto truncate(int fd, off_t newSize) -> int;
  auto unlink(const std::string &name) -> int;

private:
  Directory *directory;
  BlockCache *cache;

  struct OpenFileEntry {
    int refcnt;
    DirPtr dirp;    
  };

  std::vector<OpenFileEntry> openFiles;

  auto open(const DirPtr &dirp) -> int;
  auto applyMoves(const std::vector<DirChangeTracker::Entry> &moves) -> void;
};
}
#endif
