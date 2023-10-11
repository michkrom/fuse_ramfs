#pragma once

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <fuse.h>
#include <fuse_lowlevel.h>
#include <unistd.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

using namespace std;

struct INode;
typedef shared_ptr<INode> inptr;

struct INode /*: public std::enable_shared_from_this<INode>*/ {
  fuse_ino_t m_inode{};
  std::string m_name;
  mode_t m_mode{};

  operator bool() const { return m_inode > 0; }
  [[nodiscard]] bool isDir() const { return m_mode & S_IFDIR; }

  INode(const INode &) = delete;
  INode &operator=(const INode &) = delete;

  [[nodiscard]] static inptr find(fuse_ino_t inode) {
    if (auto i = s_i2i.find(inode); i != s_i2i.end())
      return i->second;
    else
      return nullptr;
  }

  static void del(fuse_ino_t in) { s_i2i.erase(in); }

  virtual ~INode() {}

protected:
  INode(string_view name_) : m_inode(++s_inodeCount), m_name(name_) {
    s_i2i[m_inode] = inptr(this);
  }

private:
  static fuse_ino_t s_inodeCount;
  static map<fuse_ino_t, inptr> s_i2i; // todo: move to INode
};

struct INodeFile : public INode {
  vector<char> content;

  INodeFile() = delete;
  INodeFile(const INodeFile &) = delete;
  INodeFile &operator=(const INodeFile &) = delete;

  explicit INodeFile(string_view name, mode_t mode = 0) : INode(name) {
    m_mode = S_IFREG | (mode ? mode : 0666);
  }
};

struct INodeDir : public INode {
  map<string_view, inptr> m_n2i;

  INodeDir() = delete;
  INodeDir(const INodeDir &) = delete;
  INodeDir &operator=(const INodeDir &) = delete;

  explicit INodeDir(string_view name, mode_t mode) : INode(name) {
    m_mode = S_IFDIR | (mode ? mode : 0755);
  }

  [[nodiscard]] inptr find(string_view fn) const {
    if (auto i = m_n2i.find(fn); i != m_n2i.end())
      return i->second;
    else
      return nullptr;
  }

  inptr addFile(string_view name, mode_t mode = 0) {
    auto p = new INodeFile(name, mode);
    return add(p->m_inode);
  }

  inptr addDir(string_view name, mode_t mode = 0) {
    auto p = new INodeDir(name, mode);
    return add(p->m_inode);
  }

  inptr unlink(string_view name) {
    if (auto in = find(name)) {
      m_n2i.erase(name);
      return in;
    }
    return nullptr;
  }

  bool del(string_view name) {
    if (auto in = unlink(name)) {
      INode::del(in->m_inode);
      return true;
    }
    return false;
  }

  bool ren(string_view name, string_view newname) {
    if (auto in = unlink(name)) {
      in->m_name = newname;
      add(in->m_inode);
      return true;
    }
    return false;
  }

  bool mov(string_view name, const shared_ptr<INodeDir> &newparent,
           string_view newname) {
    if (auto in = unlink(name)) {
      newparent->del(newname); // may not exist, fine!
      in->m_name = newname;
      newparent->add(in);
      return true;
    }
    return false;
  }

  void add(const inptr &in) { m_n2i[in->m_name] = in; }

  inptr add(fuse_ino_t inid) {
    auto in = INode::find(inid);
    assert(in);
    add(in);
    return in;
  }
};

static void ramfs_stat(inptr in, struct stat *stbuf) {
  *stbuf = {};
  stbuf->st_ino = in->m_inode;
  stbuf->st_mode = in->m_mode;
  stbuf->st_nlink = 1;
  if (auto inf = dynamic_pointer_cast<INodeFile>(in))
    stbuf->st_size = inf->content.size();
  else
    stbuf->st_size = 0;
}

static int ramfs_stat(fuse_ino_t ino, struct stat *stbuf) {
  if (auto in = INode::find(ino)) {
    ramfs_stat(in, stbuf);
    return 0;
  }
  return -1;
}
