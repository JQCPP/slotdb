#pragma once

#include "simpledb/core.hpp"
#include "simplified/record.hpp"
#include "storage/mmapfile.hpp"
#include "index/bucket.hpp"

namespace simplified {

struct DmsRecord {
  //total size of record, including header and data
  uint32_t size;
  //flag
  uint32_t flag;
  //data (JSON format)
  char data[0];
};

constexpr const char *kDmsHeaderEyecatcher = "DMSH";
constexpr size_t kDmsHeaderEyecatcherLen = 4;
constexpr uint32_t kDmsHeaderFlagNormal = 0;
constexpr uint32_t kDmsHeaderFlagDropped = 1;

struct DmsHeader {
  //DMSH
  char eye_catcher[kDmsHeaderEyecatcherLen];
  //page number
  uint32_t size;
  //flag
  uint16_t flag;
  //version number
  uint16_t version;
};

constexpr const char *kDmsPageEyecatcher = "PAGH";
constexpr size_t kDmsPageEyecatcherLen = 4;
constexpr uint32_t kDmsPageFlagNormal = 0;
constexpr uint32_t kDmsPageFlagUnalloc = 1;
constexpr uint32_t kSlotEmpty = 0xFFFFFFFF;

constexpr size_t kDmsExtendSize = 65536ul;
constexpr size_t kDmsPageSize = 1024ul * 1024ul * 4;
constexpr size_t kDmsMaxRecord =
    (kDmsPageSize - sizeof(DmsHeader) - sizeof(DmsRecord) - sizeof(unsigned int));
constexpr size_t kDmsMaxPages = 1024 * 256ul;
constexpr unsigned int kInvalidSlotID = 0xFFFFFFFF;
constexpr unsigned int kInvalidPageID = 0xFFFFFFFF;

constexpr uint32_t kDmsRecordFlagNormal = 0;
constexpr uint32_t kDmsRecordFlagDropped = 1;
constexpr uint32_t kDmsHeaderVersion = 0;

constexpr size_t kDmsFileSegmentSize = 1024ul * 1024ul * 128;
constexpr size_t kDmsFileHeaderSize = 1024ul * 64;
constexpr size_t kDmsPagesPerSegment = kDmsFileSegmentSize / kDmsPageSize;
constexpr size_t kDmsMaxSegments = kDmsPagesPerSegment / kDmsPagesPerSegment;

constexpr const char *kDmsKeyFieldName = "_id";

using SlotOff = unsigned int;

struct DmsPageHeader {
  //PAGE
  char eye_catcher[kDmsPageEyecatcherLen];
  //page size
  uint32_t size;
  //flag
  uint32_t flag;
  //num slots
  uint32_t num_slots;
  //slot offset (low address)
  uint32_t slot_offset;
  //free space offset (high address)
  uint32_t free_space;
  uint32_t free_offset;
  //slot data
  char data[0];
};

class DmsFile : public MmapFile {
 public:
  explicit DmsFile(std::shared_ptr<BucketManager> &bucket_manager);
  ~DmsFile();
  int initialize(const char *filename);
  int insert(const nlohmann::json &record, nlohmann::json &out_record,
           DmsRecordID &rid);
  int remove(const DmsRecordID &rid);
  int find(const DmsRecordID &rid, nlohmann::json &result);

  size_t getNumSegments() const;
  size_t getNumPages() const;
  char *pageToOffset(PageID page_id);
  bool validSize(size_t size);

 private:
 // DMS header
  DmsHeader *header_;
  //all segments address
  std::vector<char *> segments_;
  //all free space map and sorted by size
  std::multimap<uint16_t, PageID> free_space_map_;
  //read and write lock for file
  boost::shared_mutex shared_mutex_;
  //file name
  std::string file_name_;
  //index manager
  std::shared_ptr<BucketManager> bucket_manager_;

  int extendSegment();
  int initNew();
  int extendFile(int size);
  int mapSegments();
  int loadData(char *segment);
  int searchSlot(char *page, const DmsRecordID &record_id, SlotOff &slot);
  void recoverSpace(char *page);
  void updateFreeSpace(DmsPageHeader *header, int change_size, PageID page_id);
  PageID findPage(size_t required_size);
};

}  // namespace simplified