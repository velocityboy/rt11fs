#ifndef __FILEDATASOURCE_H_
#define __FILEDATASOURCE_H_

#include "DataSource.h"

namespace RT11FS {
class FileDataSource : public DataSource {
public:
  FileDataSource(int fd);
  ~FileDataSource();

  auto stat(struct stat *st) -> int override;
  auto read(void *buffer, size_t bytes, off_t offset) -> ssize_t override;  
  auto write(void *buffer, size_t bytes, off_t offset) -> ssize_t override;  

private:
  int fd;
};
}


#endif
