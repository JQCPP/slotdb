#pragma once

#include "simpledb/core.hpp"
#include "common/options.hpp"
#include "storage/dms.hpp"
#include "index/bucket.hpp"

namespace simplified {

class PmdManager {
 public:
  static PmdManager &getPmdManager();
  void init(const Options &options);
  std::shared_ptr<DmsFile> getDmsFile() const { return dms_file_; }
  std::shared_ptr<BucketManager> getBucketManager() const { return bucket_manager_; }
  const Options &options() const { return options_; }

 private:
  PmdManager();
  PmdManager(const PmdManager &) = delete;
  PmdManager &operator=(const PmdManager &) = delete;

  Options options_;
  std::shared_ptr<DmsFile> dms_file_;
  std::shared_ptr<BucketManager> bucket_manager_;
};

}  // namespace simplified