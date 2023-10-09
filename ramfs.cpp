#define FUSE_USE_VERSION 34

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ramfs.h"

using namespace std;

fuse_ino_t INode::s_inodeCount;
map<fuse_ino_t, inptr> INode::s_i2i; // todo: move to INode
