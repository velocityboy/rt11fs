// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __MEMORYDATASOURCE_H_
#define __MEMORYDATASOURCE_H_

#include "DataSource.h"

#include <cstdint>
#include <vector>

namespace RT11FS {
class MemoryDataSource : public DataSource 
{
public:
  MemoryDataSource(size_t bytes);

  auto getData() -> std::vector<uint8_t> & { return memory; }

  auto stat(struct stat *st) -> int override;
  auto read(void *buffer, size_t bytes, off_t offset) -> ssize_t override;  
  auto write(void *buffer, size_t bytes, off_t offset) -> ssize_t override;  

private:
  std::vector<uint8_t> memory;
};
}

#endif
