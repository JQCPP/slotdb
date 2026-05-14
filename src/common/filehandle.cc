#include "common/filehandle.hpp"

#include <cerrno>
#include <cstring>

FileHandleOp::FileHandleOp() : file_handle_(INVALID_FD_HANDLE) {}

FileHandleOp::~FileHandleOp() {
  if (file_handle_ != INVALID_FD_HANDLE) {
    close(file_handle_);
    file_handle_ = INVALID_FD_HANDLE;
  }
}

int FileHandleOp::Open(const char *path) {
  do {
    file_handle_ = open(path, kFileFlag, kFileMode);
  } while (-1 == file_handle_ && errno == EINTR);
  if (file_handle_ == INVALID_FD_HANDLE) {
    return COMMON_ERROR;
  }
  file_name_ = path;
  return OK;
}

int FileHandleOp::Read(void *buf, size_t size) {
  if (!isValid()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to read file: invalid handle";
    return INVALID_FD_HANDLE;
  }
  int bytesRead = 0;
  do {
    bytesRead = read(file_handle_, buf, size);
  } while (-1 == bytesRead && errno == EINTR);
  return bytesRead;
}

int FileHandleOp::Write(const void *buf, size_t size) {
  if (!isValid()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to write file: invalid handle";
    return COMMON_ERROR;
  }
  size_t current_size = 0;
  int rc = OK;
  do {
    rc = write(file_handle_, &((const char *)buf)[current_size],
              size - current_size);
    if (rc >= 0) {
      current_size += rc;
    }
  } while ((-1 == rc && errno == EINTR) ||
           (-1 != rc && current_size < size));
  if (rc == -1) {
    BOOST_LOG_TRIVIAL(error) << "Failed to write file: " << strerror(errno);
    return COMMON_ERROR;
  }
  return OK;
}

offsetType FileHandleOp::getCurrentOffset() const {
  return lseek(file_handle_, 0, SEEK_CUR);
}

void FileHandleOp::seekToOffset(offsetType offset) {
  if (offsetType(-1) != offset) {
    lseek(file_handle_, offset, SEEK_SET);
  }
}

void FileHandleOp::seekToEnd() {
  lseek(file_handle_, 0, SEEK_END);
}

offsetType FileHandleOp::getSize() const {
  struct stat fd_info;
  if (COMMON_ERROR == fstat(file_handle_, &fd_info)) {
    BOOST_LOG_TRIVIAL(error) << "Failed to get file size";
    return COMMON_ERROR;
  }
  return fd_info.st_size;
}

void FileHandleOp::setFileHandleOp(handleType handle) {
  if (isValid()) {
    close(file_handle_);
  }
  file_handle_ = handle;
}

handleType FileHandleOp::handle() const {
  return file_handle_;
}

bool FileHandleOp::isValid() const {
  return file_handle_ != INVALID_FD_HANDLE;
}