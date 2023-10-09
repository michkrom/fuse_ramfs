#define FUSE_USE_VERSION 31

#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fuse.h>
#include <iostream>
#include <stddef.h>
#include <stdio.h>

#include "ramfs.h"
#include <string.h>

#define TRACE(path) std::cout << __FUNCTION__ << " " << path << std::endl;

using namespace std;

std::shared_ptr<INodeDir> root{};

shared_ptr<INodeDir> findDirByPath(std::filesystem::path fullpath) {
  if (fullpath == "/")
    return root;
  if (auto grandparent = findDirByPath(fullpath.parent_path())) {
    return dynamic_pointer_cast<INodeDir>(
        grandparent->find(fullpath.filename().c_str()));
  }
  return nullptr;
}

inptr findByPath(string_view path) {
  std::filesystem::path fspath(path);
  if (fspath == "/")
    return root;
  if (auto parentDir = findDirByPath(fspath.parent_path()))
    return parentDir->find(fspath.filename().c_str());
  return nullptr;
}

static void *ramfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  TRACE("")
  (void)conn;
  cfg->kernel_cache = 1;
  return NULL;
}

static int ramfs_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
  TRACE(path)
  (void)fi;
  int res = -ENOENT;
  *stbuf = {};
  if (auto in = findByPath(path)) {
    ramfs_stat(in, stbuf);
    res = 0;
  }
  return res;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
  TRACE(path)
  (void)offset;
  (void)fi;
  (void)flags;

  if (auto inDir = dynamic_pointer_cast<INodeDir>(findByPath(path))) {
    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    for (const auto &in : inDir->m_n2i) {
      filler(buf, in.second->m_name.c_str(), NULL, 0, (fuse_fill_dir_flags)0);
    }
    return 0;
  }

  return -ENOENT;
}

static int ramfs_open(const char *path, struct fuse_file_info *fi) {
  TRACE(path)
  if (auto in = findByPath(path)) {
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
      return -EACCES;
  } else
    return -ENOENT;
  return 0;
}

static int ramfs_create(const char *path, mode_t mode,
                        struct fuse_file_info *fi) {
  TRACE(path)
  (void)fi;
  std::filesystem::path fspath(path);
  if (auto in = findDirByPath(fspath.parent_path())) {
    if (in->addFile(fspath.filename().c_str(), mode))
      return 0;
    else
      return -EEXIST;
  } else
    return -ENOENT;
  return 0;
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  TRACE(path)
  (void)fi;
  if (auto in = dynamic_pointer_cast<INodeFile>(findByPath(path))) {
    auto len = in->content.size();
    if (offset < len) {
      if (offset + size > len)
        size = len - offset;
      memcpy(buf, &in->content[offset], size);
    }
  }
  return size;
}

static int ramfs_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
  TRACE(path)
  (void)fi;
  if (auto in = dynamic_pointer_cast<INodeFile>(findByPath(path))) {
    in->content.resize(std::max(in->content.size(), size + offset));
    memcpy(&in->content[offset], buf, size);
  }
  return size;
}

static int ramfs_mkdir(const char *path, mode_t mode) {
  std::filesystem::path fspath(path);
  if (auto parentDir = findDirByPath(fspath.parent_path())) {
    if (parentDir->addDir(fspath.filename().c_str(), mode))
      return 0;
    else
      return -EEXIST;
  }
  return -ENOENT;
}

static int ramfs_unlink(const char *path) {
  std::filesystem::path fspath(path);
  if (auto parentDir = findDirByPath(fspath.parent_path())) {
    if (parentDir->del(fspath.filename().c_str()))
      return 0;
    else
      return -EEXIST;
  }
  return -ENOENT;
}

struct fuse *fuse{};
struct fuse_session *se{};

int mount(struct fuse_args args, const char *mountpoint) {
  int res = -1;
  static struct fuse_operations ramfs_oper {};
  ramfs_oper.init = ramfs_init;
  ramfs_oper.readdir = ramfs_readdir;
  ramfs_oper.open = ramfs_open;
  ramfs_oper.create = ramfs_create;
  ramfs_oper.getattr = ramfs_getattr;
  ramfs_oper.read = ramfs_read;
  ramfs_oper.write = ramfs_write;
  ramfs_oper.mkdir = ramfs_mkdir;
  ramfs_oper.rmdir = ramfs_unlink;
  ramfs_oper.unlink = ramfs_unlink;

  {
    auto p = new INodeDir("/", 0755);
    root = dynamic_pointer_cast<INodeDir>(INode::find(p->m_inode));
    root->addFile("qwer");
  }

  fuse = fuse_new(&args, &ramfs_oper, sizeof(ramfs_oper), nullptr);
  if (fuse == nullptr)
    goto out1;

  if (fuse_mount(fuse, mountpoint) != 0) {
    res = 4;
    goto out2;
  }

  se = fuse_get_session(fuse);
  if (fuse_set_signal_handlers(se) != 0) {
    res = 6;
    goto out3;
  }

  res = fuse_loop(fuse);
  if (res)
    res = 8;

  fuse_remove_signal_handlers(se);

out3:
  fuse_unmount(fuse);
out2:
  fuse_destroy(fuse);
out1:
  fuse_opt_free_args(&args);
  return res;
}

void umount() {
  if (fuse)
    fuse_unmount(fuse);
  root.reset();
}
