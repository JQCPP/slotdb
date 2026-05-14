#include "pmd/pmd.hpp"

namespace simplified {

PmdManager::PmdManager() : dms_file_(nullptr), bucket_manager_(nullptr) {}

PmdManager &PmdManager::getPmdManager() {
  // Meyers' Singleton pattern, thread-safe in C++11 and later
  static PmdManager instance;
  return instance;
}

void PmdManager::init(const Options &options) {
  options_ = options;
  // Initialize bucket manager
  bucket_manager_ = std::make_shared<BucketManager>();
  bucket_manager_->initialize();
  // Initialize DMS file
  dms_file_ = std::make_shared<DmsFile>(bucket_manager_);
  dms_file_->initialize(options.dataPath().c_str());
}

}  // namespace simplified