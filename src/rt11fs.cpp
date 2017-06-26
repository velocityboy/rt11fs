#include "FileSystem.h"

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
};

static auto getFS()
{
  return reinterpret_cast<FileSystem*>(fuse_get_context()->private_data);
}

auto rt11_getattr(const char *path, struct stat *stbuf) -> int
{
  return getFS()->getattr(path, stbuf);
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

auto rt11_read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  return getFS()->read(path, buf, count, offset, fi);
}

auto build_oper(struct fuse_operations *oper)
{
  oper->getattr = &rt11_getattr;
  oper->readdir = &rt11_readdir;
  oper->open = &rt11_open;
  oper->read = &rt11_read;
}

auto usage(const string &program)
{
  cerr << "usage: " << program << " disk-image mountpoint" << endl;
  exit(1);
}

struct fuse_opt rt11_opts[] = 
{
  { "-i %s", offsetof(struct rt11_config, image), 0 },
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

  fuse_opt_add_arg(&args, "-odefault_permissions");

  FileSystem fs {config.image};

  build_oper(&rt11_oper);
  exitcode = fuse_main(args.argc, args.argv, &rt11_oper, &fs);

  fuse_opt_free_args(&args);
  return exitcode;
}