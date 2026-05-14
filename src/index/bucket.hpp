#pragma once

#include "simpledb/core.hpp"
#include "simplified/record.hpp"
#include "common/hash_code.hpp"
#include "common/hash_code.hpp"

namespace simplified {

// bucket size, a prime number to reduce hash collisions
constexpr size_t kBucketSize = 1117;
// key field name for index, using "_id" as the default key field name
constexpr const char *kKeyFieldName = "_id";

struct ElementHash {
  //_id
  std::vector<char> data;
  //record id , record index in dms
  DmsRecordID record_id;
};

class BucketManager {
 private:
  class Bucket {
   private:
   //hash value, element hash code
    std::multimap<unsigned int, ElementHash> bucket_map_;
    // read write lock for bucket map
    mutable boost::shared_mutex shared_mutex_;

   public:
    int isIDExist(uint16_t num, ElementHash &element);
    int createIndex(uint16_t num, ElementHash &element);
    int findIndex(uint16_t num, ElementHash &element);
    int removeIndex(uint16_t num, ElementHash &element);
  };
  // buckets_ size is kBucketSize, each bucket is a shared pointer to a Bucket object
  std::vector<std::shared_ptr<Bucket>> buckets_;

  int processData(const nlohmann::json &record, const DmsRecordID &record_id,
                  uint16_t &hash_num, ElementHash &element, uint16_t &random);

 public:
  BucketManager();
  ~BucketManager();
  int initialize();
  int isIDExist(const nlohmann::json &record);
  int createIndex(const nlohmann::json &record, DmsRecordID &record_id);
  int findIndex(const nlohmann::json &record, DmsRecordID &record_id);
  int removeIndex(const nlohmann::json &record, DmsRecordID &record_id);
};

}  // namespace simplified