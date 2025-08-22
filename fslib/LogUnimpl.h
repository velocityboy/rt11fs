// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#ifndef __LOGUNIMPL_H_
#define __LOGUNIMPL_H_

#if defined(__linux__)
  #include <fuse3/fuse.h>
#else
  #include <fuse.h>
#endif

extern void add_unimpl(struct fuse_operations *oper);

#endif
