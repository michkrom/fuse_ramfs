#define FUSE_USE_VERSION 31

#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fuse.h>
#include <fuse_lowlevel.h> // for opts
#include <iostream>
#include <stddef.h>
#include <stdio.h>

#include "fuse_ramfs.h"
#include "ramfs.h"

int main(int argc, char *argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_cmdline_opts opts {};
  int ret = -1;

  if (fuse_parse_cmdline(&args, &opts) != 0)
    return 1;

  if (opts.show_help) {
    printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
    fuse_cmdline_help();
    fuse_lowlevel_help();
    return 0;
  } else if (opts.show_version) {
    printf("FUSE library version %s\n", fuse_pkgversion());
    fuse_lowlevel_version();
    return 0;
  }

  if (opts.mountpoint == NULL) {
    printf("usage: %s [options] <mountpoint>\n", argv[0]);
    printf("       %s --help\n", argv[0]);
    ret = 1;
    return 1;
  }

  printf("fuse hello: mounting...\n");
  ret = mount(args, opts.mountpoint);
  printf("fuse hello: done...\n");
  
  free(opts.mountpoint);
  // crashes on this: fuse_opt_free_args(&args);

  return ret;
}
