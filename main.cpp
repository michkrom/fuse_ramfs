#define FUSE_USE_VERSION 34

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fuse_lowlevel.h>
#include <iostream>
#include <vector>

#include "ramfs.h"

#define TRACE(ino) std::cout << __FUNCTION__ << " " << ino << std::endl;

shared_ptr<INodeDir> root;

static int hello_stat(fuse_ino_t ino, struct stat *stbuf) {
  if (auto f = INode::find(ino)) {
    *stbuf = {};
    stbuf->st_ino = f->m_inode;
    stbuf->st_mode = f->m_mode;
    stbuf->st_nlink = 1;
    stbuf->st_size =
        f->isDir() ? 0 : static_pointer_cast<INodeFile>(f)->content.size();
    return 0;
  }
  return -1;
}

static void ramfs_getattr(fuse_req_t req, fuse_ino_t ino,
                          [[maybe_unused]] struct fuse_file_info *fi) {
  struct stat stbuf {};
  if (hello_stat(ino, &stbuf) == 0)
    fuse_reply_attr(req, &stbuf, 1.0);
  else
    fuse_reply_err(req, ENOENT);
}

static void ramfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
  TRACE(parent)

  if (auto ind = static_pointer_cast<INodeDir>(INode::find(parent))) {
    if (auto in = ind->find(name)) {
      struct fuse_entry_param e {};
      e.ino = in->m_inode;
      e.attr_timeout = 1.0;
      e.entry_timeout = 1.0;
      hello_stat(e.ino, &e.attr);
      fuse_reply_entry(req, &e);
      return;
    }
  }
  fuse_reply_err(req, ENOENT);
}

struct dirbuf {
  char *p;
  size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, fuse_ino_t ino) {
  if (auto in = INode::find(ino)) {
    struct stat stbuf {};
    stbuf.st_ino = ino;
    stbuf.st_mode = in->m_mode;
    size_t oldsize = b->size;
    b->size += fuse_add_direntry(req, NULL, 0, in->m_name.c_str(), NULL, 0);
    b->p = (char *)realloc(b->p, b->size);
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize,
                      in->m_name.c_str(), &stbuf, b->size);
  }
}

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
                             off_t off, size_t maxsize) {
  if (off < bufsize)
    return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
  else
    return fuse_reply_buf(req, NULL, 0);
}

static void ramfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off,
                          [[maybe_unused]] struct fuse_file_info *fi) {
  TRACE(ino)

  if (auto inDir = static_pointer_cast<INodeDir>(INode::find(ino))) {
    struct dirbuf b {};
    for (const auto &in : inDir->m_n2i) {
      dirbuf_add(req, &b, in.second->m_inode);
    }
    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);
  } else {
    fuse_reply_err(req, ENOTDIR);
  }
}

static void ramfs_open(fuse_req_t req, fuse_ino_t ino,
                       struct fuse_file_info *fi) {
  TRACE(ino)
  // else if ((fi->flags & O_ACCMODE) != O_RDONLY)
  // 	fuse_reply_err(req, EACCES);
  if (auto in = INode::find(ino); in && !in->isDir()) {
    fuse_reply_open(req, fi);
  } else {
    fuse_reply_err(req, ENOENT);
  }
}

static void ramfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                       [[maybe_unused]] struct fuse_file_info *fi) {
  TRACE(ino)
  if (auto in = static_pointer_cast<INodeFile>(INode::find(ino)))
    reply_buf_limited(req, in->content.data(), in->content.size(), off, size);
  else
    fuse_reply_err(req, ENOENT);
}

static void ramfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                        size_t size, off_t off,
                        [[maybe_unused]] struct fuse_file_info *fi) {
  TRACE(ino)

  if (auto in = static_pointer_cast<INodeFile>(INode::find(ino))) {
    size_t newsize = max(in->content.size(), off + size);
    in->content.resize(newsize);
    memcpy(&in->content[off], buf, size);
    fuse_reply_write(req, size);
  } else
    fuse_reply_err(req, ENOENT);
}

static void ramfs_create(fuse_req_t req, fuse_ino_t parent, const char *name,
                         mode_t mode,
                         [[maybe_unused]] struct fuse_file_info *fi) {
  TRACE(parent)
  fuse_entry_param ep{};
  if (auto in = static_pointer_cast<INodeDir>(INode::find(parent))) {
    auto newin = in->addFile(name, mode);
    ep.ino = newin->m_inode;
    hello_stat(ep.ino, &ep.attr);
    fuse_reply_create(req, &ep, fi);
  } else
    fuse_reply_err(req, ENOENT);
}

static void ramfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
  TRACE(parent)
  if (auto in = static_pointer_cast<INodeDir>(INode::find(parent))) {
    if (in->del(name)) {
      fuse_reply_err(req, 0);
      return;
    }
  }
  fuse_reply_err(req, ENOENT);
}

static void ramfs_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
                         fuse_ino_t newparent, const char *newname,
                         [[maybe_unused]] unsigned int flags) {
  TRACE(parent)
  TRACE(newparent)

  auto ret = EINVAL;

  auto indold = static_pointer_cast<INodeDir>(INode::find(parent));
  auto indnew = static_pointer_cast<INodeDir>(INode::find(newparent));

  if (indold && indnew) {
    if (indold == indnew) {
      ret = indold->ren(name, newname) ? 0 : ENOENT;
    } else
      ret = indold->mov(name, indnew, newname) ? 0 : ENOENT;
  }
  fuse_reply_err(req, ret);
}

void ramfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
                 mode_t mode) {
  TRACE(parent)

  if (auto parentin = static_pointer_cast<INodeDir>(INode::find(parent))) {
    if (parentin->addDir(name, mode)) {
      fuse_reply_err(req, 0);
      return;
    }
  }
  fuse_reply_err(req, ENOENT);
}

void ramfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
  ramfs_unlink(req, parent, name);
}

void ramfs_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                    size_t size) {
  TRACE(ino);
  fuse_reply_err(req, ENOSYS);
}

void ramfs_getxattrlist(fuse_req_t req, fuse_ino_t ino, size_t size) {
  TRACE(ino);
  fuse_reply_err(req, ENOSYS);
}

struct fuse_session *se = nullptr;

int mount(struct fuse_args args, const char *mountpoint) {
  int ret = -1;
  static struct fuse_lowlevel_ops ramfs_oper {};
  ramfs_oper.lookup = ramfs_lookup;
  ramfs_oper.getattr = ramfs_getattr;
  ramfs_oper.readdir = ramfs_readdir;
  ramfs_oper.create = ramfs_create;
  ramfs_oper.open = ramfs_open;
  ramfs_oper.read = ramfs_read;
  ramfs_oper.write = ramfs_write;
  ramfs_oper.unlink = ramfs_unlink;
  ramfs_oper.rename = ramfs_rename;
  ramfs_oper.mkdir = ramfs_mkdir;
  ramfs_oper.rmdir = ramfs_rmdir;
  ramfs_oper.getxattr = ramfs_getxattr;
  ramfs_oper.listxattr = ramfs_getxattrlist;

  {
    auto p = new INodeDir("/", 0755);
    root = static_pointer_cast<INodeDir>(INode::find(p->m_inode));
    root->addFile("qwer");
  }

  se = fuse_session_new(&args, &ramfs_oper, sizeof(ramfs_oper), nullptr);
  if (se == nullptr)
    goto err_out1;

  if (fuse_set_signal_handlers(se) != 0)
    goto err_out2;

  if (fuse_session_mount(se, mountpoint) != 0)
    goto err_out3;

  // block until unmounted
  ret = fuse_session_loop(se);

  fuse_session_unmount(se);

err_out3:
  fuse_remove_signal_handlers(se);

err_out2:
  fuse_session_destroy(se);
  se = 0;

err_out1:
  return ret ? 1 : 0;
}

void umount() {
  if (se)
    fuse_session_unmount(se);
}

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
  fuse_opt_free_args(&args);

  root.reset();

  return ret;
}