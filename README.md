# SimpleDB

A lightweight C++17 Key-Value store built on **mmap memory mapping** and a **page-based slot storage engine**, using Boost.Asio for TCP communication and nlohmann/json for serialization.

## Architecture

```
Client / Shell                    ← Client Layer (C++ shell / Go client)
    ↓
TCP Network Layer (Boost.Asio)    ← Network Communication
    ↓
Message Handler + Command Dispatch ← Protocol Parsing + Command Pattern
    ↓
PMD (Process Manager)            ← Process Management Singleton
    ↓
┌─────────────────────┐
│   BucketManager     │  ← Hash Index (1117 buckets)
│   (Index Layer)     │
└────────┬────────────┘
┌────────┴────────────┐
│   DmsFile (Storage) │  ← mmap persistence + page slot management
│   MmapFile          │
│   FileHandleOp      │
└─────────────────────┘
```

## Quick Start

### Dependencies

- C++17 compiler
- Boost (system, thread, log, program_options)
- nlohmann/json (header-only)

### Build

```bash
cd SimpleDB
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run

```bash
# Start the server
./simpledb --port 27017 --datapath ./data

# Start the interactive shell (another terminal)
./simpledb_shell
```

Command-line options:

| Option | Default | Description |
|--------|---------|-------------|
| `--port` / `-p` | 27017 | Listening port |
| `--pool` / `-t` | 4 | Thread pool size |
| `--datapath` / `-d` | ./data | Data file path |
| `--log` / `-l` | ./log.cfg | Log configuration file |

Shell commands:

```
insert {"_id":"abc", "name":"hello", "value":123}
query {"_id":"abc"}
delete {"_id":"abc"}
help
quit
```

---

## Data Layer In Depth

The data layer consists of three classes, bottom-up: `FileHandleOp` → `MmapFile` → `DmsFile`.

### Layer 1: FileHandleOp — File Descriptor Wrapper

**Files:** `src/common/filehandle.hpp/cc`

A thin wrapper around POSIX file I/O:

```cpp
class FileHandleOp {
    int Open(const char *path);           // open(path, O_RDWR|O_CREAT, 0644)
    int Write(const void *buf, size_t);   // loop write() until all bytes written
    int Read(void *buf, size_t);          // read()
    offsetType getSize() const;           // fstat() to get file size
    void seekToEnd();                     // lseek(SEEK_END)
    void seekToOffset(offsetType);        // lseek(SEEK_SET)
    handleType handle() const;            // returns the raw fd
};
```

- `O_RDWR | O_CREAT`: open in read-write mode, create if not exists
- File permissions: `0644` (`S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH`)
- All syscalls handle `EINTR` signal interruption with retry
- Destructor automatically calls `close(fd)`

### Layer 2: MmapFile — Memory Mapping Wrapper

**Files:** `src/storage/mmapfile.hpp/cc`

Maps a file into the process address space via `mmap`. Writes to mapped memory are synced to disk by the OS automatically.

```cpp
class MmapFile {
    bool opened_;
    FileHandleOp file_handle_;
    std::vector<MmapSegment> segments_;   // all mmap segments

    struct MmapSegment {
        void *ptr;      // mapping start address
        size_t length;  // mapping length
        size_t offset;  // offset within the file
    };
};
```

Core methods:

| Method | Behavior |
|--------|----------|
| `Open(path)` | `file_handle_.Open(path)` opens the file |
| `Map(offset, length, &ptr)` | If file is too small, pad with `malloc+Write` → `mmap(addr=NULL, length, PROT_READ\|PROT_WRITE, MAP_SHARED, fd, offset)` → return mapped address, record in `segments_` |
| `Close()` | Iterate `segments_` and call `munmap` on all |

Key roles of `mmap` in this project:

1. **No manual buffer management** — direct pointer access after mapping, OS page cache handles everything
2. **Lazy sync** — with `MAP_SHARED`, the kernel's pdflush thread writes dirty pages back at appropriate times
3. **Demand paging** — only accessed pages are loaded into physical memory
4. **Crash semantics** — unwritten data is lost on crash, same as an unflushed write

### Layer 3: DmsFile — Data Management Engine (Core)

**Files:** `src/storage/dms.hpp/cc`

Inherits `MmapFile` and implements a complete page-based slot storage engine.

#### Class Member Overview

```cpp
class DmsFile : public MmapFile {
    // Pointer to file header (points into mmap region)
    DmsHeader *header_;

    // Start addresses of all segments
    std::vector<char *> segments_;

    // Global free space index — key=page free bytes, value=page_id
    // Uses multimap, automatically sorted by free space ascending
    std::multimap<uint16_t, PageID> free_space_map_;

    // Read-write lock — insert/remove exclusive write, find shared read
    boost::shared_mutex shared_mutex_;
};
```

---

#### Data Structure Definitions

##### DmsHeader — File Header (64KB)

Located at the start of the file, records global metadata:

```cpp
struct DmsHeader {
    char     eye_catcher[4];   // "DMSH" — magic number for format validation
    uint32_t size;             // total number of pages
    uint16_t flag;             // status flag
    uint16_t version;          // format version (current = 0)
};
// Actual size = kDmsFileHeaderSize = 64KB (space reserved beyond this header)
```

##### DmsPageHeader — Page Header

At the start of each page, manages allocation state:

```cpp
struct DmsPageHeader {
    char     eye_catcher[4];   // "PAGH" — page magic number
    uint32_t size;             // page size = 4MB
    uint32_t flag;             // kDmsPageFlagNormal(0) / kDmsPageFlagUnalloc(1)
    uint32_t num_slots;        // number of currently allocated slots
    uint32_t slot_offset;      // write offset for next slot (top of slot area)
    uint32_t free_space;       // remaining free bytes
    uint32_t free_offset;      // write offset for next record (bottom of data area)
    char     data[0];          // flexible array — start of actual data area
};
```

##### DmsRecord — Record Header

Metadata prepended to each persisted record:

```cpp
struct DmsRecord {
    uint32_t size;             // total size of this record (including DmsRecord itself)
    uint32_t flag;             // kDmsRecordFlagNormal(0) / kmsRecordFlagDropped(1)
    char     data[0];          // flexible array — JSON string
};
```

##### DmsRecordID — Record Locator

Used to pass record positions between index and storage:

```cpp
struct DmsRecordID {
    PageID page_id;   // unsigned int, page number
    SlotID slot_id;   // unsigned int, slot number
};
```

---

#### File Layout

```
Offset 0:
┌──────────────────────────────────────────────────────┐
│                 DmsHeader (64KB)                     │
│  [0x00] eye_catcher: "DMSH\0\0\0\0" (4B)           │
│  [0x04] size: total page count       (4B)            │
│  [0x08] flag: 0                     (2B)            │
│  [0x0A] version: 0                  (2B)            │
│  [0x0C ... 0xFFFF] reserved padding (64KB - 12B)    │
├──────────────────────────────────────────────────────┤
│                 Segment 0 (128MB)                     │
│  ├── Page 0   (4MB) ──────────────────────────────┤ │
│  ├── Page 1   (4MB) ──────────────────────────────┤ │
│  ├── ...                                           │ │
│  └── Page 31  (4MB) ──────────────────────────────┤ │
├──────────────────────────────────────────────────────┤
│                 Segment 1 (128MB)                     │
│  ...                                                  │
└──────────────────────────────────────────────────────┘
```

**Constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `kDmsFileHeaderSize` | 64 KB | File header space |
| `kDmsFileSegmentSize` | 128 MB | Size of each Segment |
| `kDmsPageSize` | 4 MB | Size of each Page |
| `kDmsPagesPerSegment` | 32 | Pages per Segment |
| `kDmsExtendSize` | 64 KB | Minimum file extension granularity |
| `kDmsMaxRecord` | ≈ 4MB - overhead | Maximum record size |

---

#### Page Internal Layout — Bidirectional Slot System

The internal structure of each 4MB Page:

```
Low Address
  ┌──────────────────────────────────────┐  ← page start address
  │  DmsPageHeader (28B fixed part)      │
  │  - eye_catcher: "PAGH"               │
  │  - size: 0x400000 (4MB)              │
  │  - flag: 0                           │
  │  - num_slots: 3                      │  ← 3 slots currently
  │  - slot_offset: 40                   │  ← next slot writes here
  │  - free_space: 4194256               │  ← remaining free bytes
  │  - free_offset: 4194296              │  ← next record writes here
  ├──────────────────────────────────────┤
  │  Slot array (grows ↑ from low addr)  │
  │  [0] offset_of_record_0  (4B)       │  ← points to an offset within the page
  │  [1] offset_of_record_1  (4B)       │
  │  [2] offset_of_record_2  (4B)       │
  │  [3] next slot position (will write)│
  ├──────────────────────────────────────┤
  │         Free Area                    │
  │    free_space bytes                  │
  │                                      │
  ├──────────────────────────────────────┤
  │  Data area (grows ↓ from high addr)  │
  │  + Record_2 (most recent) ─────────+ │
  │  | DmsRecord {size, flag}          | │
  │  | JSON: '{"_id":"c",...}'        | │
  │  +--------------------------------+ │
  │  + Record_1 ──────────────────────+ │
  │  | ...                            | │
  │  +--------------------------------+ │
  │  + Record_0 (earliest) ───────────+ │
  │  | ...                            | │
  │  +--------------------------------+ │
  └──────────────────────────────────────┘
High Address
```

**Key Design Principles:**

- `slot_offset` is the **write pointer** for the slot array (low address boundary of free area)
- `free_offset` is the **write pointer** for the data area (high address boundary of free area)
- `free_space = free_offset - slot_offset` — bytes of contiguous free space
- When `free_space` is insufficient, `recoverSpace()` is tried first; if still not enough, a new page is found

**Record layout after insertion:**

```
Entry in slot array:
    slot[i] = byte offset of the record within the page (relative to page start)

Record in page data area:
    [DmsRecord 8B] [JSON string + '\0']
     ├─ size = sizeof(DmsRecord) + json_len + 1
     ├─ flag = 0 (Normal)
     └─ data = full JSON dump
    
slot[i] ──points to──→ [DmsRecord.size] [DmsRecord.flag] [JSON data\0]
                        ←── record_storage_size = sizeof(DmsRecord) + json_len + 1 ──→
```

---

#### Free Space Management — `free_space_map_`

```cpp
std::multimap<uint16_t, PageID> free_space_map_;
```

A global multimap sorted by free space **ascending**:

| key | value | Meaning |
|-----|-------|---------|
| `page_header->free_space` (uint16_t) | PageID | Remaining free space of that page |

**`findPage(need_size)` algorithm:**

```
1. auto iter = free_space_map_.upper_bound(need_size)
2. if iter != end:
       return iter->second   // found a page with enough space
   else:
       return kInvalidPageID // no page available, need extendSegment()
```

Why `upper_bound`? Because the multimap key is the exact free byte count. `upper_bound(need_size)` returns the first element with key **strictly `> need_size`**, i.e. a page with space just enough to fit the request, avoiding excessive waste.

When a page's `free_space` changes (decreases on insert, increases on remove), the old entry must be removed from the map and a new one inserted — handled by `updateFreeSpace()`:

```
void updateFreeSpace(header, change_size, page_id):
    1. old_free = header->free_space
    2. find all page_ids matching old_free in the multimap
    3. erase the matching entry
    4. header->free_space += change_size
    5. insert new entry (new_free_space → page_id)
```

---

#### Core Operations In Detail

##### `initialize(filename)` — Initialization

```
DmsFile::initialize(filename):
  │
  ├─ MmapFile::Open(filename)
  │   └─ FileHandleOp::Open(path)  →  open(path, O_RDWR|O_CREAT, 0644)
  │
  ├─ file_handle_.getSize()
  │   ├─ = 0 (new file) → initNew():
  │   │     ├─ extendFile(kDmsFileHeaderSize)
  │   │     │   └─ write zero-filled blocks in 64KB granularity via seekToEnd + Write
  │   │     ├─ Map(0, 64KB, &header_)  →  mmap the file header
  │   │     └─ write magic "DMSH" + size=0 + flag=0 + version=0
  │   └─ > 0 (existing file) → skip
  │
  └─ mapSegments():
        ├─ if header not yet mapped → Map(0, 64KB, &header_)
        ├─ num_pages = header_->size
        ├─ num_segments = num_pages / 32
        ├─ for each segment i:
        │     Map(kDmsFileHeaderSize + 128MB * i, 128MB, &segment)
        │     segments_.push_back(segment)
        │     loadData(segment)
        └─ return

loadData(segment):
    for each page in segment:
        1. insert page's free_space into free_space_map_
        2. iterate all slots:
            if slot[i] != kSlotEmpty:
                read JSON, call bucket_manager_->createIndex() to rebuild index
```

##### `insert(record)` — Insert

```
DmsFile::insert(record, out_record, rid):
  │
  ├─ 1. Calculate requirements
  │    record_size = json.dump().size()
  │    record_storage_size = record_size + 1           // +1 for trailing '\0'
  │    need_size = record_storage_size + sizeof(DmsRecord) + sizeof(SlotID)
  │    if record_size == 0 → ErrPackParse
  │    if record_size > kDmsMaxRecord → ErrInvaildArg
  │
  ├─ 2. Acquire write lock
  │    boost::unique_lock<boost::shared_mutex>
  │
  ├─ 3. Find an available Page
  │    retry:
  │    page_id = findPage(need_size)
  │    if kInvalidPageID:
  │        extendSegment()  →  allocate a new segment
  │        goto retry
  │
  ├─ 4. Locate the Page
  │    page = pageToOffset(page_id)
  │    page_header = (DmsPageHeader *)page
  │
  ├─ 5. Fragmentation check and compaction
  │    if page_header->free_space > page_header->free_offset - page_header->slot_offset:
  │        recoverSpace(page)    // compact records within the page
  │        // after compaction, slot_offset and free_offset are corrected
  │
  ├─ 6. Secondary space check
  │    if page_header->free_space < need_size:
  │        update free_space_map_ with the latest free_space
  │        goto retry  // find another page
  │
  ├─ 7. Write data
  │    offset_tmp = free_offset - record_storage_size - sizeof(DmsRecord)
  │    (a) write slot entry: slot[num_slots] = offset_tmp
  │    (b) memcpy DmsRecord header (size + flag)
  │    (c) memcpy JSON string
  │
  ├─ 8. Update metadata
  │    rid = {page_id, num_slots}
  │    page_header->num_slots++
  │    page_header->slot_offset += sizeof(SlotID)
  │    page_header->free_offset = offset_tmp
  │    updateFreeSpace(page_header, -(record_storage_size + sizeof(SlotID) + sizeof(DmsRecord)), page_id)
  │
  └─ 9. Return OK
```

**`extendSegment()` — Allocating a new segment:**

```
extendSegment():
  1. get current file end offset
  2. extendFile(kDmsFileSegmentSize) → expand file by 128MB in 64KB zero-filled blocks
  3. Map(offset, 128MB, &segment) → mmap the new segment
  4. iterate all 32 pages in this segment:
     (a) memcpy an initialized DmsPageHeader
         - eye_catcher = "PAGH"
         - size = 4MB
         - flag = Normal
         - num_slots = 0
         - slot_offset = sizeof(DmsPageHeader)
         - free_space = 4MB - sizeof(DmsPageHeader)
         - free_offset = 4MB
     (b) insert each page's free_space into free_space_map_
  5. segments_.push_back(segment)
  6. header_->size += 32
```

##### `find(rid)` — Query

```
DmsFile::find(rid, result):
  │
  ├─ 1. Acquire read lock
  │    boost::shared_lock<boost::shared_mutex>
  │
  ├─ 2. Locate Page
  │    page = pageToOffset(rid.page_id)
  │    if page == nullptr → ErrSys
  │
  ├─ 3. Find Slot
  │    searchSlot(page, rid, slot):
  │        page_header = (DmsPageHeader *)page
  │        if rid.slot_id >= page_header->num_slots → ErrSys
  │        slot = *(SlotOff *)(page + sizeof(DmsPageHeader) + rid.slot_id * sizeof(SlotOff))
  │
  ├─ 4. Validate
  │    if slot == kSlotEmpty (0xFFFFFFFF) → ErrSys
  │
  ├─ 5. Read record header
  │    record_header = (DmsRecord *)(page + slot)
  │    if record_header->flag == kDmsRecordFlagDropped → ErrSys
  │
  ├─ 6. Parse JSON
  │    data = page + slot + sizeof(DmsRecord)
  │    result = nlohmann::json::parse(data)
  │
  └─ 7. Return OK
```

**`pageToOffset(page_id)` — PageID to memory address conversion:**

```cpp
char *DmsFile::pageToOffset(PageID page_id) {
    if (page_id >= getNumPages()) return nullptr;
    size_t segment_index = page_id / kDmsPagesPerSegment;    // which segment
    size_t page_in_segment = page_id % kDmsPagesPerSegment;  // page within segment
    return segments_[segment_index] + kDmsPageSize * page_in_segment;
}
```

##### `remove(rid)` — Delete (Logical Deletion)

```
DmsFile::remove(rid):
  │
  ├─ 1. Acquire write lock
  │
  ├─ 2. Locate Page + Slot
  │
  ├─ 3. slot[rid.slot_id] = kSlotEmpty (0xFFFFFFFF)
  │    // slot marked empty, record is "logically deleted"
  │
  ├─ 4. record_header->flag = kDmsRecordFlagDropped
  │    // record header marked as dropped
  │
  ├─ 5. updateFreeSpace(page_header, +record_header->size, page_id)
  │    // reclaim space (data still physically present, can be overwritten on next insert)
  │
  └─ 6. Return OK
```

**Note: remove does NOT physically erase data.** It:
- Sets the slot entry to `0xFFFFFFFF` (the slot position can be reused on next insert)
- Sets the record header flag to Dropped
- Returns the occupied space to `free_space_map_`

---

#### `recoverSpace()` — Compaction Algorithm

When `page_header->free_space > page_header->free_offset - page_header->slot_offset`, there is fragmentation within the page (holes caused by logical deletion). Compaction is triggered:

```
Input: page
       left  = page + sizeof(DmsPageHeader)    // start of slot area
       right = page + kDmsPageSize              // end of data area

Algorithm:
  valid_count = 0

  1. Iterate all slot[i]:
     if slot[i] != kSlotEmpty:
         record = page + slot[i]
         rec_size = record->size
         right -= rec_size
         memmove(right, page + slot[i], rec_size)   // move to high address end
         slot[valid_count] = (right - page)          // update slot to new position
         valid_count++

  2. Update PageHeader:
     num_slots   = valid_count
     slot_offset = sizeof(DmsPageHeader) + valid_count * sizeof(SlotOff)
     free_offset = (right - page)
     free_space  = free_offset - slot_offset
```

**Before vs after compaction:**

```
Before compaction (slot 1 deleted):
  ┌─────┬────┬────┬──────┬─────────────────────────────┐
  │ HDR │ S0 │ S1 │ S2   │        free area             │  R2  │  R0  │
  │     │ ●  │ ∅  │ ●    │                             │  ●   │  ●   │
  └─────┴────┴────┴──────┴─────────────────────────────┘
                  ↑ slot_offset               free_offset ↑

After compaction:
  ┌─────┬────┬────┬─────────────────────────────────────┐
  │ HDR │ S0 │ S1 │              free area               │  R2  │  R0  │
  │     │ ●  │ ●  │                                      │  ●   │  ●   │
  └─────┴────┴────┴─────────────────────────────────────┘
              ↑ slot_offset                    free_offset ↑
  slot[0] still points to R0, slot[1] points to R2 (above R0)
  All holes eliminated, free_space restored to a contiguous region
```

---

#### Full Insert Sequence Diagram

```
InsertCommand::execute(json)
  │
  ├─ PmdManager::getDmsFile()  →  get the DmsFile singleton
  │
  ├─ dms->insert(input, output, rid)
  │   │
  │   ├─ [WRITE LOCK] boost::unique_lock
  │   │
  │   ├─ findPage(need_size)
  │   │   ├─ upper_bound(need_size) in free_space_map_
  │   │   ├─ found → return page_id
  │   │   └─ not found → extendSegment()
  │   │         ├─ extendFile(128MB) → write zero data
  │   │         ├─ mmap new segment
  │   │         ├─ initialize 32 PageHeaders
  │   │         ├─ insert into free_space_map_
  │   │         ├─ segments_.push_back
  │   │         └─ retry findPage
  │   │
  │   ├─ pageToOffset(page_id)
  │   │   └─ segments_[page_id/32] + (page_id%32) * 4MB
  │   │
  │   ├─ recoverSpace()  if fragmentation exists
  │   │
  │   ├─ slot[num_slots] = free_offset - record_size - sizeof(DmsRecord)
  │   ├─ memcpy(DmsRecord header)
  │   ├─ memcpy(JSON data)
  │   ├─ update PageHeader metadata
  │   ├─ updateFreeSpace() → update free_space_map_
  │   └─ [WRITE UNLOCK]
  │
  └─ bucket->createIndex(output, rid)
      │
      ├─ extract _id from json
      ├─ HashCode(_id)  →  DJB2 hash
      ├─ hash % 1117    →  select bucket
      ├─ [WRITE LOCK on bucket]
      ├─ bucket_map_.insert(hash_num, {_id_data, rid})
      └─ [WRITE UNLOCK]
```

---

#### Concurrency Control

Uses `boost::shared_mutex` for read-write locking:

| Operation | Lock Type | Concurrency |
|-----------|-----------|-------------|
| `insert()` | `unique_lock` (write lock) | Mutual exclusion, blocks all other insert/find |
| `remove()` | `unique_lock` (write lock) | Mutual exclusion |
| `find()` | `shared_lock` (read lock) | Multiple find can run concurrently, mutually exclusive with write lock |

Additionally, the `MmapFile` layer uses its own `std::mutex` to protect the `segments_` vector, but `DmsFile` locking is coarser (file-level).

---

## Index Layer

**Files:** `src/index/bucket.hpp/cc`

Hash bucket index for accelerating `_id` lookups.

### Data Structures

```cpp
constexpr size_t kBucketSize = 1117;  // prime number to reduce hash collisions

struct ElementHash {
    std::vector<char> data;   // string value of the _id field
    DmsRecordID record_id;    // corresponding record location in the storage layer
};

class Bucket {
    std::multimap<unsigned int, ElementHash> bucket_map_;
    // key = hash_num (DJB2 hash value)
    // value = {_id raw value, record position}
    mutable boost::shared_mutex shared_mutex_;
};

class BucketManager {
    std::vector<std::shared_ptr<Bucket>> buckets_;  // 1117 buckets
};
```

### Index Operations

| Operation | Flow |
|-----------|------|
| `createIndex(json, rid)` | Extract `json["_id"]` → DJB2 hash → `hash % 1117` select bucket → insert into multimap |
| `findIndex(json, &rid)` | Same bucket selection → `equal_range(hash)` locate → linear compare `_id` raw value → return rid |
| `removeIndex(json, rid)` | Same locate → erase matching element |

**DJB2 Hash Algorithm:**

```cpp
uint32_t HashCode(const char *str, size_t len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i];
    return hash;
}
```

### Index vs Storage Data Flow

```
Insert:  Storage writes data first → gets rid → Index saves rid
Query:   Index finds rid first → Storage reads data by rid
Delete:  Index finds rid first → Storage deletes data → Index deletes entry
```

This design **decouples** index from storage: the storage layer has no knowledge of index structure, and the index layer does not care about physical data layout.

---

## Network Layer

**Files:** `src/server/main.cc`

```cpp
// Main loop pseudocode
boost::asio::io_service io_service;
tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port));

while (!gQuit) {
    auto socket = make_shared<tcp::socket>(io_service);
    acceptor.accept(*socket);                       // blocking wait for connection

    pool.execute([socket]() {                       // thread pool handles it
        while (!disconnect) {
            socket->read_some(buf);                  // read request
            HandleMessage(buf, &len, &disconnect);   // process message
            boost::asio::write(socket, reply);       // write response
        }
    });
}
```

Each client connection is dispatched to a thread pool task, which loops processing requests until the client sends `quit`.

---

## Message Layer

**Files:** `src/server/handle_message.cc`

```
HandleMessage(input, len, disconnect, output):
  1. parse JSON from input_str
  2. extract "cmd" field
  3. split cmd by space → fields[]
  4. command.commandProcesser(fields) → ICommand*
  5. if ICommand* found:
         icmd->execute(data, output_json)
     else:
         output_json["error"] = "unknown command"
  6. if cmd == "quit": *disconnect = true
  7. output_json.dump() → reply bytes
```

## Command Layer

**Files:** `src/command/command.hpp/cc`, `src/command/icommand.hpp/cc`

Uses the **Command design pattern**:

```
Command class (registry)
  cmd_map_ = {
    "insert"  → InsertCommand
    "connect" → ConnectCommand
    "query"   → QueryCommand
    "delete"  → DeleteCommand
    "help"    → HelpCommand
    "test"    → TestCommand
  }

commandProcesser(fields)  →  lookup table, return ICommand*
                                ↓
ICommand::execute(json_input, json_output)  ←  virtual polymorphism
```

## Project Structure

```
SimpleDB/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── log.cfg
│
├── include/
│   ├── simpledb/
│   │   └── core.hpp                 # Common header + error codes + constants
│   └── simplified/
│       ├── client.hpp               # Client class declaration (TCP client)
│       ├── shell.hpp                # Shell class declaration (interactive client)
│       ├── record.hpp               # DmsRecordID + PageID + SlotID types
│       ├── rtn.hpp                  # kOk / kError constants
│       └── simpledb.hpp             # Simple C API declaration
│
├── src/
│   ├── server/                      # ===== Network + Message Layer =====
│   │   ├── main.cc                  # Server entry point, TCP accept loop
│   │   ├── handle_message.cc        # Message parsing + command dispatch
│   │   ├── handle_message.hpp
│   │   ├── threadpool.cc            # Thread pool implementation
│   │   └── threadpool.hpp
│   │
│   ├── pmd/                         # ===== Process Management Layer =====
│   │   ├── pmd.cc                   # PmdManager singleton
│   │   └── pmd.hpp                  # Holds DmsFile + BucketManager
│   │
│   ├── command/                     # ===== Command Layer =====
│   │   ├── icommand.cc              # Insert/Query/Delete/Connect/Help/Test implementations
│   │   ├── icommand.hpp             # ICommand interface + command class declarations
│   │   ├── command.cc               # Command registry (cmd → ICommand*)
│   │   └── command.hpp
│   │
│   ├── storage/                     # ===== Storage Layer (Core) =====
│   │   ├── dms.cc                   # DmsFile page-based storage engine
│   │   ├── dms.hpp                  # DmsHeader/PageHeader/Record definitions
│   │   ├── mmapfile.cc              # MmapFile mmap wrapper
│   │   └── mmapfile.hpp
│   │
│   ├── index/                       # ===== Index Layer =====
│   │   ├── bucket.cc                # BucketManager hash index
│   │   └── bucket.hpp
│   │
│   └── common/                      # ===== Common Infrastructure =====
│       ├── client.cc                # Client::Impl TCP client (PIMPL)
│       ├── shell.cc                 # Shell interactive loop implementation
│       ├── message.cc               # JSON ↔ byte sequence conversion
│       ├── message.hpp
│       ├── options.cc               # Boost.Program_options parsing
│       ├── options.hpp
│       ├── logging.cc               # Boost.Log initialization (console + file)
│       ├── logging.hpp
│       ├── hash_code.cc             # DJB2 hash algorithm
│       ├── hash_code.hpp
│       ├── cmdline.cc               # String split + trim utilities
│       ├── cmdline.hpp
│       ├── filehandle.cc            # POSIX open/read/write/lseek wrapper
│       ├── filehandle.hpp
│       ├── simpledb.cc              # Simple C API implementation (stub)
│       └── simpledb.hpp             # (deprecated old header)
│
├── client/                          # Go language client
│   ├── main.go
│   ├── message.go
│   ├── db.cc                        # msgpack serialization example
│   └── main_test.go
│
├── shell.cc                         # simpledb_shell entry point main()
│
├── simpledb.hpp                     # Legacy C++ client class (deprecated)
└── client.cc                        # Legacy C++ client example
```
