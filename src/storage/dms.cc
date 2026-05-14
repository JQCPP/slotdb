#include "storage/dms.hpp"

#include <cstring>

namespace simplified {

namespace {
nlohmann::json JsonToMsgpack(const nlohmann::json &json) {
  return json;
}
nlohmann::json MsgpackToJson(const char *data) {
  return nlohmann::json::parse(data);
}
}  // namespace

DmsFile::DmsFile(std::shared_ptr<BucketManager> &bucket_manager)
    : header_(nullptr), bucket_manager_(bucket_manager) {}

DmsFile::~DmsFile() {}

int DmsFile::initialize(const char *file_name) {
  int rc = OK;
  file_name_ = file_name;
  //open file
  rc = Open(file_name);
  if (rc != OK) return rc;
getfilesize:
  auto offset = file_handle_.getSize();
  if (!offset) {
    rc = initNew();
    if (rc != OK) return rc;
    goto getfilesize; // retry to get file size
  }
  //map file all segments
  rc = mapSegments();
  if (rc != OK) return rc;
  return OK;
}

int DmsFile::initNew() {
  //create file header
  auto rc = extendFile(kDmsFileHeaderSize);
  if (rc != OK) return rc;
  //map header
  rc = Map(0, kDmsFileHeaderSize, (void **)&header_);
  if (rc != OK) return rc;
  //set header info
  std::strcpy(header_->eye_catcher, kDmsHeaderEyecatcher);
  header_->size = 0;
  header_->flag = kDmsHeaderFlagNormal;
  header_->version = kDmsHeaderVersion;
  return OK;
}

int DmsFile::extendFile(int size) {
  char temp[kDmsExtendSize] = {0};
  if (size % kDmsExtendSize != 0) return ErrSys;
  for (size_t i = 0; i < (size_t)size; i += kDmsExtendSize) {
    file_handle_.seekToEnd();
    auto len = file_handle_.Write(temp, kDmsExtendSize);
    if (len != OK) return ErrSys;
  }
  return OK;
}

int DmsFile::extendSegment() {
  auto rc = OK;
  auto offset = file_handle_.getSize();
  //extend file size
  rc = extendFile(kDmsFileSegmentSize);
  if (rc != OK) return rc;
  char *segment = nullptr;
  //map new segment
  rc = Map(offset, kDmsFileSegmentSize, (void **)&segment);
  if (rc != OK) return rc;

  //init new segment
  DmsPageHeader page_header;
  std::strcpy(page_header.eye_catcher, kDmsPageEyecatcher);
  page_header.size = kDmsPageSize;
  page_header.flag = kDmsPageFlagNormal;
  page_header.num_slots = 0;
  page_header.slot_offset = sizeof(DmsPageHeader);
  page_header.free_space = kDmsPageSize - sizeof(DmsPageHeader);
  page_header.free_offset = kDmsPageSize;
  for (size_t i = 0; i < kDmsFileSegmentSize; i += kDmsPageSize) {
    std::memcpy(segment + i, &page_header, sizeof(DmsPageHeader));
  }

  auto free_map_size = free_space_map_.size();
  for (size_t i = 0; i < kDmsPagesPerSegment; ++i) {
    free_space_map_.insert(
        std::make_pair(page_header.free_space, (PageID)(i + free_map_size)));
  }
  segments_.push_back(segment);
  header_->size += kDmsPagesPerSegment;
  return OK;
}

PageID DmsFile::findPage(size_t require_size) {
  //find page with enough free space
  auto iter = free_space_map_.upper_bound(require_size);
  return (iter != free_space_map_.end()) ? iter->second : kInvalidPageID;
}

int DmsFile::searchSlot(char *page, const DmsRecordID &rid, SlotOff &slot) {
  if (!page) return ErrSys;
  auto page_header = (DmsPageHeader *)page;
  if (rid.slot_id > page_header->num_slots) return ErrSys;
  slot = *(SlotOff *)(page + sizeof(DmsPageHeader) +
                      rid.slot_id * sizeof(SlotOff));
  return OK;
}

void DmsFile::recoverSpace(char *page) {
  auto left = page + sizeof(DmsPageHeader);
  auto right = page + kDmsPageSize;  
  auto page_header = (DmsPageHeader *)page;
  
  size_t valid_slot_count = 0;
  
  for (size_t i = 0; i < page_header->num_slots; ++i) {
    auto slot = *((SlotOff *)(left + sizeof(SlotOff) * i));
    
    if (kSlotEmpty != slot) {
      auto record_header = (DmsRecord *)(page + slot);
      auto record_size = record_header->size;
      
      right -= record_size;   
      std::memmove(right, page + slot, record_size);

      *((SlotOff *)(left + sizeof(SlotOff) * valid_slot_count)) = 
          (SlotOff)(right - page);
      
      valid_slot_count++;
    }
  }
  
  page_header->num_slots = valid_slot_count;
  page_header->slot_offset = sizeof(DmsPageHeader) + valid_slot_count * sizeof(SlotOff);
  page_header->free_offset = (uint32_t)(right - page);  
  page_header->free_space = page_header->free_offset - page_header->slot_offset;
}

void DmsFile::updateFreeSpace(DmsPageHeader *header, int change_size,
                             PageID page_id) {
                
  //erase old free space map
  auto free_space = header->free_space;
  auto ret = free_space_map_.equal_range(free_space);
  for (auto it = ret.first; it != ret.second; ++it) {
    if (it->second == page_id) {
      free_space_map_.erase(it);
      break;
    }
  }
  //update free space map
  free_space += change_size;
  header->free_space = free_space;
  //insert new free space map
  free_space_map_.insert(std::pair<uint16_t, PageID>(free_space, page_id));
}

int DmsFile::insert(const nlohmann::json &record, nlohmann::json &out_record,
                    DmsRecordID &rid) {
  auto record_size = record.dump().size();
  if (record_size == 0) return ErrPackParse;
  if (record_size > kDmsMaxRecord) return ErrInvaildArg;

  boost::unique_lock<boost::shared_mutex> write_lock(shared_mutex_);

  auto record_storage_size = record_size + 1; // +1 for null terminator
  auto need_size = record_storage_size + sizeof(DmsRecord) + sizeof(SlotID);

retry:
  auto page_id = findPage(need_size);
  if (kInvalidPageID == page_id) {
    auto rc = extendSegment();
    if (rc != OK) return rc;
    goto retry;
  }
  auto page = pageToOffset(page_id);
  if (!page) return ErrSys;
  auto page_header = (DmsPageHeader *)page;
  auto old_free_space = page_header->free_space;
  if (page_header->free_space >
        page_header->free_offset - page_header->slot_offset) {
    recoverSpace(page);
  }
  if (page_header->free_space < need_size) {
    auto ret = free_space_map_.equal_range(old_free_space);
    for (auto it = ret.first; it != ret.second; ++it) {
      if (it->second == page_id) {
        free_space_map_.erase(it);
        break;
      }
    }
    free_space_map_.insert(
        std::make_pair(page_header->free_space, page_id));
    goto retry;
  }
  auto offset_tmp = page_header->free_offset - record_storage_size - sizeof(DmsRecord);
  DmsRecord record_header;
  record_header.size = record_storage_size + sizeof(DmsRecord);
  record_header.flag = kDmsRecordFlagNormal;

  *(SlotOff *)(page + sizeof(DmsPageHeader) +
              page_header->num_slots * sizeof(SlotOff)) = offset_tmp;
  std::memcpy(page + offset_tmp, &record_header, sizeof(DmsRecord));
  auto store_position = page + offset_tmp + sizeof(DmsRecord);
  std::memcpy(store_position, record.dump().c_str(), record_storage_size);
  out_record = record;

  rid.page_id = page_id;
  rid.slot_id = page_header->num_slots;
  page_header->num_slots++;
  page_header->slot_offset += sizeof(SlotID);
  page_header->free_offset = offset_tmp;
  updateFreeSpace(page_header,
                  -(record_storage_size + sizeof(SlotID) + sizeof(DmsRecord)),
                  page_id);
  return OK;
}

int DmsFile::remove(const DmsRecordID &rid) {
  boost::unique_lock<boost::shared_mutex> write_lock(shared_mutex_);
  auto page = pageToOffset(rid.page_id);
  if (!page) return ErrSys;

  SlotOff slot = 0;
  auto rc = searchSlot(page, rid, slot);
  if (rc != OK) return rc;
  if (kSlotEmpty == slot) return ErrSys;

  auto page_header = (DmsPageHeader *)page;
  //update slot to empty
  *(SlotID *)(page + sizeof(DmsPageHeader) + rid.slot_id * sizeof(SlotID)) =
      kSlotEmpty;
  auto record_header = (DmsRecord *)(page + slot);
  record_header->flag = kDmsRecordFlagDropped;
  //update free space
  updateFreeSpace(page_header, record_header->size, rid.page_id);
  return OK;
}

int DmsFile::find(const DmsRecordID &rid, nlohmann::json &result) {
  //read lock
  boost::shared_lock<boost::shared_mutex> read_lock(shared_mutex_);
  auto page = pageToOffset(rid.page_id);
  if (!page) return ErrSys;

  SlotOff slot;
  //search slot
  auto rc = searchSlot(page, rid, slot);
  if (kSlotEmpty == slot) return ErrSys;

  //get record header
  auto record_header = (DmsRecord *)(page + slot);
  if (kDmsRecordFlagDropped == record_header->flag) return ErrSys;
  //get data
  auto data = page + slot + sizeof(DmsRecord);
  result = nlohmann::json::parse(data);
  return OK;
}

int DmsFile::loadData(char *segment) {
  //each segmen has 128 pages, each page has 4096 bytes
  for (size_t i = 0; i < kDmsPagesPerSegment; ++i) {
    //get page header
    auto page_header = (DmsPageHeader *)(segment + i * kDmsPageSize);
    //insert page to free space map
    free_space_map_.insert(
        std::pair<unsigned int, PageID>(page_header->free_space, (PageID)i));
    //get slot number
    auto slot_id = (SlotID)page_header->num_slots;
    //set record id
    DmsRecordID record_id;
    record_id.page_id = (PageID)i;
    //for each slot
    for (size_t s = 0; s < slot_id; ++s) {
      //get each slot offset
      auto slot_offset =
          *(SlotOff *)(segment + i * kDmsPageSize + sizeof(DmsPageHeader) +
                       s * sizeof(SlotID));
      //slot is empty 0xFFFFFFFF, continue
      if (kSlotEmpty == slot_offset) {
        continue;
      }
      //get data 
      auto data = segment + i * kDmsPageSize + slot_offset + sizeof(DmsRecord);
      //get json object from data
      auto json_obj = nlohmann::json::parse(data);
      //set record id.page_id 
      record_id.slot_id = (SlotID)s;
      //insert index to bucket manager
      bucket_manager_->createIndex(json_obj, record_id);
    }
  }
  return OK;
}

int DmsFile::mapSegments() {
  if (!header_) {
    //map header
    auto rc = Map(0, kDmsFileHeaderSize, (void **)&header_);
    if (rc != OK) return rc;
  }
  //read header
  auto num_page = header_->size;
  //culculate num segments
  auto num_segments = num_page / kDmsPagesPerSegment;
  if (num_segments > 0) {
    //map segments
    for (size_t i = 0; i < (size_t)num_segments; ++i) {
      char *segment = nullptr;
      auto rc = Map(kDmsFileHeaderSize + kDmsFileSegmentSize * i,
                  kDmsFileSegmentSize, (void **)&segment);
      if (rc != OK) return rc;
      segments_.push_back(segment);
      //load data
      loadData(segment);
    }
  }
  return OK;
}

size_t DmsFile::getNumSegments() const {
  return segments_.size();
}

size_t DmsFile::getNumPages() const {
  return getNumSegments() * kDmsPagesPerSegment;
}

char *DmsFile::pageToOffset(PageID page_id) {
  if (page_id >= getNumPages()) {
    return nullptr;
  }
  return segments_[page_id / kDmsPagesPerSegment] +
         kDmsPageSize * (page_id % kDmsPagesPerSegment);
}

bool DmsFile::validSize(size_t size) {
  if (size < kDmsFileHeaderSize) {
    return false;
  }
  size -= kDmsFileHeaderSize;
  return (size % kDmsFileSegmentSize != 0) ? false : true;
}

}  // namespace simplified