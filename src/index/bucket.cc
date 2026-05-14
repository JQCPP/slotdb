#include "index/bucket.hpp"

#include <algorithm>

namespace simplified {

BucketManager::BucketManager() {}

BucketManager::~BucketManager() {}

int BucketManager::initialize() {
  for (size_t i = 0; i < kBucketSize; ++i) {
    buckets_.emplace_back(std::make_shared<Bucket>());
  }
  return OK;
}

int BucketManager::isIDExist(const nlohmann::json &record) {
  uint16_t hash_num = 0;
  uint16_t random = 0;
  ElementHash hash_element;
  DmsRecordID record_id;
  auto rc = processData(record, record_id, hash_num, hash_element, random);
  if (rc != OK) return rc;
  return buckets_[random]->isIDExist(hash_num, hash_element);
}

int BucketManager::createIndex(const nlohmann::json &record,
                             DmsRecordID &record_id) {
  uint16_t hash_num = 0;
  uint16_t random = 0;
  ElementHash element;
  //calculate hash value of key field and get bucket index by mod operation
  auto rc = processData(record, record_id, hash_num, element, random);
  if (rc != OK) return rc;

  // create index in bucket and return record id
  rc = buckets_[random]->createIndex(hash_num, element);
  if (rc != OK) return rc;
  record_id = element.record_id;
  return OK;
}

int BucketManager::findIndex(const nlohmann::json &record,
                          DmsRecordID &record_id) {
  uint16_t hash_num = 0;
  uint16_t random = 0;
  ElementHash element;
  //calculate hash value of key field and get bucket index by mod operation
  auto rc = processData(record, record_id, hash_num, element, random);
  if (rc != OK) return rc;
  // find index in bucket and return record id
  rc = buckets_[random]->findIndex(hash_num, element);
  if (rc != OK) return rc;
  record_id = element.record_id;
  return OK;
}

int BucketManager::removeIndex(const nlohmann::json &record,
                            DmsRecordID &record_id) {
  uint16_t hash_num = 0;
  uint16_t random = 0;
  ElementHash element;
  //calculate hash value of key field and get bucket index by mod operation
  auto rc = processData(record, record_id, hash_num, element, random);
  if (rc != OK) return rc;
  //remove index in bucket and return record id
  rc = buckets_[random]->removeIndex(hash_num, element);
  if (rc != OK) return rc;
  record_id = element.record_id;
  return OK;
}

int BucketManager::processData(const nlohmann::json &record,
                               const DmsRecordID &record_id,
                               uint16_t &hash_num, ElementHash &element,
                               uint16_t &random) {
  std::string elem;
  try {
    // get key field from record which is structrued JSON object
    elem = record[kKeyFieldName];
  } catch (nlohmann::json::exception &e) {
    BOOST_LOG_TRIVIAL(error) << e.what();
    return ErrInvaildArg;
  }
  //calculate hash value of key field and get bucket index by mod operation
  hash_num = HashCode(elem.c_str(), elem.size());
  random = hash_num % kBucketSize;
  // copy key field to element
  std::copy(elem.begin(), elem.end(), std::back_inserter(element.data));
  element.record_id = record_id;
  return OK;
}

int BucketManager::Bucket::isIDExist(uint16_t const hash_num,
                                    ElementHash &element) {
  boost::shared_lock<boost::shared_mutex> read_lock(shared_mutex_);
  auto ret = bucket_map_.equal_range(hash_num);
  for (auto it = ret.first; it != ret.second; ++it) {
    auto &exist_element = it->second;
    if (exist_element.data.size() == element.data.size() &&
        exist_element.data == element.data) {
      return ErrIDExist;
    }
  }
  return OK;
}

int BucketManager::Bucket::createIndex(uint16_t hash_num,
                                     ElementHash &element) {

  boost::unique_lock<boost::shared_mutex> write_lock(shared_mutex_);
  //insert element to bucket map
  bucket_map_.insert(std::pair<uint16_t, ElementHash>(hash_num, element));
  return OK;
}

int BucketManager::Bucket::findIndex(uint16_t hash_num, ElementHash &element) {
  //read lock for bucket map
  boost::shared_lock<boost::shared_mutex> read_lock(shared_mutex_);
  //find all elements with the same hash value, then compare key field value to find the right element  
  auto ret = bucket_map_.equal_range(hash_num);
  for (auto it = ret.first; it != ret.second; ++it) {
    auto &exist_element = it->second;
    if (exist_element.data.size() == element.data.size() &&
        exist_element.data == element.data) {
      element.record_id = exist_element.record_id;
      return OK;
    }
  }
  return ErrIDNotExist;
}

int BucketManager::Bucket::removeIndex(uint16_t hash_num,
                                     ElementHash &element) {
  boost::unique_lock<boost::shared_mutex> write_lock(shared_mutex_);
  //find all elements with the same hash value, then compare key field value to find the right element  
  auto ret = bucket_map_.equal_range(hash_num);
  for (auto it = ret.first; it != ret.second; ++it) {
    auto &exist_element = it->second;
    if (exist_element.data == element.data) {
      element.record_id = exist_element.record_id;
      bucket_map_.erase(it);
      return OK;
    }
  }
  return ErrIDNotExist;
}

}  // namespace simplified