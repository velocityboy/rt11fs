// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __DIRECTORYBUILDER_H_
#define __DIRECTORYBUILDER_H_

#include "Block.h"
#include "DirConst.h"
#include "MemoryDataSource.h"
#include "Rad50.h"

#include <algorithm>
#include <cstdint>
#include <vector>

class DirectoryBuilder
{
public:
  DirectoryBuilder(RT11FS::MemoryDataSource &dataSource);

  static const uint16_t REST_OF_DATA = 0xffff;

  struct DirEntry {
    uint16_t status;
    RT11FS::Dir::Rad50Name name;
    uint16_t length;
    uint8_t job;
    uint8_t channel;
    uint16_t creation;

    DirEntry(
      uint16_t status = RT11FS::Dir::E_EOS, 
      uint16_t length = 0, 
      const RT11FS::Dir::Rad50Name &name = RT11FS::Dir::Rad50Name {}, 
      uint8_t job = 0, 
      uint8_t channel = 0, 
      uint16_t creation = 0
    ) : status(status)
      , length(length)
      , job(job)
      , channel(channel)
      , creation(creation)
    {
      std::copy(begin(name), end(name), begin(this->name));
    }
  };

  auto formatEmpty(int dirSegments, int extraBytes = 0) -> void;
  auto formatWithEntries(int dirSegments, const std::vector<std::vector<DirEntry>> &entries, int extraBytes = 0) -> void; 

private:
  RT11FS::MemoryDataSource &dataSource;
  std::vector<uint8_t> &data;

  auto putWord(int offset, uint16_t word) -> void;
  auto putEntry(int segment, int index, const DirEntry &entry, int extraBytes = 0) -> void;
};

#endif
