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

void ramfs_stat(inptr in, struct stat *stbuf) {
  *stbuf = {};
  stbuf->st_ino = in->m_inode;
  stbuf->st_mode = in->m_mode;
  stbuf->st_nlink = 1;
  if (auto inf = dynamic_pointer_cast<INodeFile>(in))
    stbuf->st_size = inf->content.size();
  else
    stbuf->st_size = 0;
}

int ramfs_stat(fuse_ino_t ino, struct stat *stbuf) {
  if (auto in = INode::find(ino)) {
    ramfs_stat(in, stbuf);
    return 0;
  }
  return -1;
}
