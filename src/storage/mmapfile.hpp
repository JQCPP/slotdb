#pragma once

#include "common/filehandle.hpp"

namespace simplified {

class MmapFile {
 public:
  explicit MmapFile();
  ~MmapFile();
  int Open(const char *path);
  int Close();
  int Map(offsetType offset, size_t length, void **address);
  bool isOpen() const;

 protected:
  bool opened_;
  FileHandleOp file_handle_;
  std::string file_name_;

  struct MmapSegment {
    void *ptr;
    size_t length;
    size_t offset;
  };
  std::vector<MmapSegment> segments_;
  std::mutex mutex_;
};

}  // namespace simplified