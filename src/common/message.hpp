#pragma once

#include "simpledb/core.hpp"

namespace simplified {

std::vector<uint8_t> JsonToMsgpack(const nlohmann::json &json);
nlohmann::json MsgpackToJson(const char *data);

}  // namespace simplified