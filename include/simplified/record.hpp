#pragma once

#include "simpledb/core.hpp"

using PageID = unsigned int;
using SlotID = unsigned int;

struct DmsRecordID {
  PageID page_id;
  SlotID slot_id;
};