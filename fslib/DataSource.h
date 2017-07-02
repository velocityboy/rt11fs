#ifndef __DATASOURCE_H_
#define __DATASOURCE_H_

#include <cstdio>
#include <sys/stat.h>

namespace RT11FS {
class DataSource
{
public:
  /**
   * Follows the semantics of stat(2).
   *
   * Returns information about the underlying data source as if it
   * were a file (which it usually is).
   *
   * @param st a buffer to fill with status data.
   * @return 0 on success or a negated errno
   */
  virtual auto stat(struct stat *st) -> int = 0;

  /**
   * Follows the semantics of pread(2).
   *
   * @param buffer the buffer to read into.
   * @param offset the offset into the data source to read from.
   * @param bytes the number of bytes to read.
   * @return the number of bytes read, or a negated errno on failure
   */
  virtual auto read(void *buffer, size_t bytes, off_t offset) -> ssize_t = 0;  

  /**
   * Follows the semantics of pwrite(2).
   *
   * @param buffer the buffer to write from.
   * @param offset the offset into the data source to write into.
   * @param bytes the number of bytes to write.
   * @return the number of bytes written, or a negated errno on failure
   */
  virtual auto write(void *buffer, size_t bytes, off_t offset) -> ssize_t = 0;
};
}

#endif
