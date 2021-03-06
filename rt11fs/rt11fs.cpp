// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#include "FileSystem.h"
#include "LogUnimpl.h"

#include <fuse.h>
#include <iostream>
#include <cstddef>
#include <string>

using RT11FS::FileSystem;

using std::cerr;
using std::endl;
using std::string;

namespace {
struct rt11_config
{
  char *image;
  int listdir;
};

static auto getFS()
{
  return reinterpret_cast<FileSystem*>(fuse_get_context()->private_data);
}

auto rt11_getattr(const char *path, struct stat *stbuf) -> int
{
  return getFS()->getattr(path, stbuf);
}

auto rt11_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) -> int
{
  return getFS()->fgetattr(path, stbuf, fi);
}

auto rt11_statfs(const char *path, struct statvfs *vfs) -> int
{
  return getFS()->statfs(path, vfs);
}

auto rt11_chmod(const char *path, mode_t mode) -> int
{
  return getFS()->chmod(path, mode);
}

auto rt11_unlink(const char *path) -> int
{
  return getFS()->unlink(path);
}

auto rt11_rename(const char *oldName, const char *newName) -> int
{ 
  return getFS()->rename(oldName, newName);
}

auto rt11_opendir(const char *, struct fuse_file_info *) -> int
{
  return 0;
}

auto rt11_releasedir(const char *, struct fuse_file_info *) -> int
{
  return 0;
}

auto rt11_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
  off_t offset, struct fuse_file_info *fi) -> int
{
  return getFS()->readdir(path, buf, filler, offset, fi);
}

auto rt11_open(const char *path, struct fuse_file_info *fi)
{
  return getFS()->open(path, fi);
}

auto rt11_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
  return getFS()->create(path, mode, fi);
}

auto rt11_release(const char *path, struct fuse_file_info *fi)
{
  return getFS()->release(path, fi);
}

auto rt11_ftruncate(const char *path, off_t size, struct fuse_file_info *fi) -> int
{
  return getFS()->ftruncate(path, size, fi);
}

auto rt11_read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  return getFS()->read(path, buf, count, offset, fi);
}

auto rt11_write(const char *path, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  return getFS()->write(path, buf, count, offset, fi); 
}

auto rt11_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
  return getFS()->fsync(path, isdatasync, fi);
}

auto build_oper(struct fuse_operations *oper)
{
  add_unimpl(oper);

  oper->getattr = &rt11_getattr;
  oper->fgetattr = &rt11_fgetattr;
  oper->statfs = &rt11_statfs;
  oper->chmod = &rt11_chmod;
  oper->unlink = &rt11_unlink;
  oper->rename = &rt11_rename;
  oper->opendir = &rt11_opendir;
  oper->releasedir = &rt11_releasedir;
  oper->readdir = &rt11_readdir;
  oper->open = &rt11_open;
  oper->create = &rt11_create;
  oper->release = &rt11_release;
  oper->ftruncate = &rt11_ftruncate;
  oper->read = &rt11_read;
  oper->write = &rt11_write;
  oper->fsync = &rt11_fsync;
}

auto usage(const string &program)
{
  cerr << "usage: " << program << " disk-image mountpoint" << endl;
  exit(1);
}

struct fuse_opt rt11_opts[] = 
{
  { "-i %s", offsetof(struct rt11_config, image), 0 },
  { "-d",    offsetof(struct rt11_config, listdir), 1},
  FUSE_OPT_END,
};

}

auto main(int argc, char *argv[]) -> int
{
  int exitcode;

  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_operations rt11_oper;
  struct rt11_config config;
  
  memset(&config, 0, sizeof(config));

  if (fuse_opt_parse(&args, &config, rt11_opts, NULL) == -1) {
    usage(argv[0]);
  }

  if (config.image == NULL) {
    cerr << argv[0] << ": must specify an image to mount" << endl;
    usage(argv[0]);
  }

  // make FUSE responsible for enforcing permission bits
  fuse_opt_add_arg(&args, "-odefault_permissions");

  // the file system isn't thread safe; make FUSE serialize requests
  // (it isn't likely that perf will be enough of an issues that this will ever matter)
  fuse_opt_add_arg(&args, "-s");

  FileSystem fs {config.image};

  if (config.listdir) {
    fs.lsdir();
    return 0;
  }

  build_oper(&rt11_oper);
  exitcode = fuse_main(args.argc, args.argv, &rt11_oper, &fs);

  fuse_opt_free_args(&args);
  return exitcode;
}