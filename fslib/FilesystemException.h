// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __FILESYSTEM_EXCEPTION
#define __FILESYSTEM_EXCEPTION

#include <stdexcept>
#include <string>

namespace RT11FS {
class FilesystemException : public std::runtime_error {
public:
  FilesystemException(int error, const std::string &what)
    : runtime_error(what)
    , error(error)
  {    
  }

  auto getError() const { return error; }

private:
  int error;
};
}

#endif