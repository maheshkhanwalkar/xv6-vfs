#pragma once
#include "vfs.h"

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct vfs_inode *ip;
  uint off;
};

#define CONSOLE 1
