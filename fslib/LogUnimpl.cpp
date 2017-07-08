// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#include "LogUnimpl.h"

#include <cerrno>
#include <iostream>

using std::cerr;
using std::endl;

static auto stub_getattr(const char *, struct stat *) -> int
{ cerr << "getattr" << endl; return -ENOSYS; }

static auto stub_readlink(const char *, char *, size_t) -> int
{ cerr << "readlink" << endl; return -ENOSYS; }

static auto stub_getdir(const char *, fuse_dirh_t, fuse_dirfil_t) -> int
{ cerr << "getdir" << endl; return -ENOSYS; }

static auto stub_mknod(const char *, mode_t, dev_t) -> int
{ cerr << "mknod" << endl; return -ENOSYS; }

static auto stub_mkdir(const char *, mode_t) -> int
{ cerr << "mkdir" << endl; return -ENOSYS; }

static auto stub_unlink(const char *) -> int
{ cerr << "unlink" << endl; return -ENOSYS; }

static auto stub_rmdir(const char *) -> int
{ cerr << "rmdir" << endl; return -ENOSYS; }

static auto stub_symlink(const char *, const char *) -> int
{ cerr << "symlink" << endl; return -ENOSYS; }

static auto stub_rename(const char *, const char *) -> int
{ cerr << "rename" << endl; return -ENOSYS; }

static auto stub_link(const char *, const char *) -> int
{ cerr << "link" << endl; return -ENOSYS; }

static auto stub_chmod(const char *, mode_t) -> int
{ cerr << "chmod" << endl; return -ENOSYS; }

static auto stub_chown(const char *, uid_t, gid_t) -> int
{ cerr << "chown" << endl; return -ENOSYS; }

static auto stub_truncate(const char *, off_t) -> int
{ cerr << "truncate" << endl; return -ENOSYS; }

static auto stub_utime(const char *, struct utimbuf *) -> int
{ cerr << "utime" << endl; return -ENOSYS; }

static auto stub_open(const char *, struct fuse_file_info *) -> int
{ cerr << "open" << endl; return -ENOSYS; }

static auto stub_read(const char *, char *, size_t, off_t, struct fuse_file_info *) -> int
{ cerr << "read" << endl; return -ENOSYS; }

static auto stub_write(const char *, const char *, size_t, off_t, struct fuse_file_info *) -> int
{ cerr << "write" << endl; return -ENOSYS; }

static auto stub_statfs(const char *, struct statvfs *fs) -> int
{ 
  cerr << "statfs" << endl;
  memset(fs, 0, sizeof(struct statvfs));
  fs->f_namemax = 255;
  fs->f_bsize = 512;

  return 0; 
}

static auto stub_flush(const char *, struct fuse_file_info *) -> int
{ cerr << "flush" << endl; return -ENOSYS; }

static auto stub_release(const char *, struct fuse_file_info *) -> int
{ cerr << "release" << endl; return -ENOSYS; }

static auto stub_fsync(const char *, int, struct fuse_file_info *) -> int
{ cerr << "fsync" << endl; return -ENOSYS; }

static auto stub_setxattr(const char *, const char *, const char *, size_t, int, uint32_t) -> int
{ cerr << "setxattr" << endl; return -ENOSYS; }

static auto stub_getxattr(const char *, const char *, char *, size_t, uint32_t) -> int
{ cerr << "getxattr" << endl; return -ENOSYS; }

static auto stub_listxattr(const char *, char *, size_t) -> int
{ cerr << "listxattr" << endl; return -ENOSYS; }

static auto stub_removexattr(const char *, const char *) -> int
{ cerr << "removexattr" << endl; return -ENOSYS; }

static auto stub_opendir(const char *, struct fuse_file_info *) -> int
{ cerr << "opendir" << endl; return 0; }

static auto stub_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *) -> int
{ cerr << "readdir" << endl; return -ENOSYS; }

static auto stub_releasedir(const char *, struct fuse_file_info *) -> int
{ cerr << "releasedir" << endl; return -ENOSYS; }

static auto stub_fsyncdir(const char *, int, struct fuse_file_info *) -> int
{ cerr << "fsyncdir" << endl; return -ENOSYS; }

static auto stub_access(const char *, int) -> int
{ cerr << "access" << endl; return -ENOSYS; }

static auto stub_create(const char *, mode_t, struct fuse_file_info *) -> int
{ cerr << "create" << endl; return -ENOSYS; }

static auto stub_ftruncate(const char *, off_t, struct fuse_file_info *) -> int
{ cerr << "ftruncate" << endl; return -ENOSYS; }

static auto stub_fgetattr(const char *, struct stat *, struct fuse_file_info *) -> int
{ cerr << "fgetattr" << endl; return -ENOSYS; }

static auto stub_lock(const char *, struct fuse_file_info *, int cmd, struct flock *) -> int
{ cerr << "lock" << endl; return -ENOSYS; }

static auto stub_utimens(const char *, const struct timespec tv[2]) -> int
{ cerr << "utimens" << endl; return -ENOSYS; }

static auto stub_bmap(const char *, size_t blocksize, uint64_t *idx) -> int
{ cerr << "bmap" << endl; return -ENOSYS; }

static auto stub_setvolname(const char *) -> int
{ cerr << "setvolname" << endl; return -ENOSYS; }

static auto stub_exchange(const char *, const char *, unsigned long) -> int
{ cerr << "exchange" << endl; return -ENOSYS; }

static auto stub_getxtimes(const char *, struct timespec *bkuptime, struct timespec *crtime) -> int
{ cerr << "getxtimes" << endl; return -ENOSYS; }

static auto stub_setbkuptime(const char *, const struct timespec *tv) -> int
{ cerr << "setbkuptime" << endl; return -ENOSYS; }

static auto stub_setchgtime(const char *, const struct timespec *tv) -> int
{ cerr << "setchgtime" << endl; return -ENOSYS; }

static auto stub_setcrtime(const char *, const struct timespec *tv) -> int
{ cerr << "setcrtime" << endl; return -ENOSYS; }

static auto stub_chflags(const char *, uint32_t) -> int
{ cerr << "chflags" << endl; return -ENOSYS; }

static auto stub_setattr_x(const char *, struct setattr_x *) -> int
{ cerr << "setattr_x" << endl; return -ENOSYS; }

static auto stub_fsetattr_x(const char *, struct setattr_x *, struct fuse_file_info *) -> int
{ cerr << "fsetattr_x" << endl; return -ENOSYS; }

void add_unimpl(struct fuse_operations *oper)
{
  oper->getattr = &stub_getattr;
  oper->readlink = &stub_readlink;
  oper->getdir = &stub_getdir;
  oper->mknod = &stub_mknod;
  oper->mkdir = &stub_mkdir;
  oper->unlink = &stub_unlink;
  oper->rmdir = &stub_rmdir;
  oper->symlink = &stub_symlink;
  oper->rename = &stub_rename;
  oper->link = &stub_link;
  oper->chmod = &stub_chmod;
  oper->chown = &stub_chown;
  oper->truncate = &stub_truncate;
  oper->utime = &stub_utime;
  oper->open = &stub_open;
  oper->read = &stub_read;
  oper->write = &stub_write;
  oper->statfs = &stub_statfs;
  oper->flush = &stub_flush;
  oper->release = &stub_release;
  oper->fsync = &stub_fsync;
  oper->setxattr = &stub_setxattr;
  oper->getxattr = &stub_getxattr;
  oper->listxattr = &stub_listxattr;
  oper->removexattr = &stub_removexattr;
  oper->opendir = &stub_opendir;
  oper->readdir = &stub_readdir;
  oper->releasedir = &stub_releasedir;
  oper->fsyncdir = &stub_fsyncdir;
  oper->access = &stub_access;
  oper->create = &stub_create;
  oper->ftruncate = &stub_ftruncate;
  oper->fgetattr = &stub_fgetattr;
  oper->lock = &stub_lock;
  oper->utimens = &stub_utimens;
  oper->bmap = &stub_bmap;
  oper->setvolname = &stub_setvolname;
  oper->exchange = &stub_exchange;
  oper->getxtimes = &stub_getxtimes;
  oper->setbkuptime = &stub_setbkuptime;
  oper->setchgtime = &stub_setchgtime;
  oper->setcrtime = &stub_setcrtime;
  oper->chflags = &stub_chflags;
  oper->setattr_x = &stub_setattr_x;
  oper->fsetattr_x = &stub_fsetattr_x;
}
