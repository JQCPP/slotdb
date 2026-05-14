#pragma once

#include "simpledb/core.hpp"

constexpr int INVALID_FD_HANDLE = -1;

using handleType = int;
using offsetType = off_t;

class FileHandleOp {
 public:
  explicit FileHandleOp();
  ~FileHandleOp();
  int Open(const char *path);
  int Read(void *buf, size_t size);
  int Write(const void *buf, size_t len);
  offsetType getCurrentOffset() const;
  void seekToOffset(offsetType offset);
  void seekToEnd();
  offsetType getSize() const;
  void setFileHandleOp(handleType handle);
  handleType handle() const;
  bool isValid() const;

 private:
  handleType file_handle_;
  std::string file_name_;
  static constexpr int kFileFlag = O_RDWR | O_CREAT;
  static constexpr int kFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
};