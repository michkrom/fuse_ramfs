#pragma once

#include <fuse.h>

int mount(struct fuse_args args, const char *mountpoint);
int unmount();
