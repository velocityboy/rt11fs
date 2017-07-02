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

  auto getData() -> uint8_t * { return &memory[0]; }

  auto stat(struct stat *st) -> int override;
  auto read(void *buffer, size_t bytes, off_t offset) -> ssize_t override;  
  auto write(void *buffer, size_t bytes, off_t offset) -> ssize_t override;  

private:
  std::vector<uint8_t> memory;
};
}

#endif
