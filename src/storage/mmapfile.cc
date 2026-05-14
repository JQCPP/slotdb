#include "storage/mmapfile.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace simplified {

MmapFile::MmapFile() : opened_(false) {}

MmapFile::~MmapFile() {
  Close();
}


//open file
int MmapFile::Open(const char *path) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_handle_.Open(path) == INVALID_FD_HANDLE) {
    BOOST_LOG_TRIVIAL(error) << "Failed to open mmapfile: " << path;
    return COMMON_ERROR;
  }
  file_name_ = path;
  opened_ = true;
  return OK;
}

int MmapFile::Close() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (opened_) {
    std::for_each(segments_.begin(), segments_.end(), [](auto &segment) {
      if (segment.ptr != nullptr) {
        munmap(segment.ptr, segment.length);
      }
    });
  }
  segments_.clear();
  file_name_.clear();
  opened_ = false;
  return OK;
}

int MmapFile::Map(offsetType offset, const size_t length, void **address) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto file_size = file_handle_.getSize();
  // expand file if necessary
  if (file_size < (offsetType)length + offset) {
    auto len = length + offset - file_size;
    auto tmp = malloc(len);
    file_handle_.Write(tmp, len);
    free(tmp);
  }
  // map file
  auto result = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                   file_handle_.handle(), offset);
  if (result == MAP_FAILED) {
    BOOST_LOG_TRIVIAL(error) << "Failed to mmap file: " << strerror(errno);
    return COMMON_ERROR;
  }
  *address = result;
  MmapSegment seg;
  seg.ptr = result;
  seg.length = length;
  seg.offset = offset;
  segments_.push_back(seg);
  return OK;
}

bool MmapFile::isOpen() const {
  return opened_;
}

}  // namespace simplified