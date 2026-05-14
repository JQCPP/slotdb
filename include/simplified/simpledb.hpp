#pragma once

#include "simpledb/core.hpp"

namespace simplified {

int SimpleDBInsert(const std::string &host, const nlohmann::json &record);
std::string SimpleDBQuery(const std::string &host, const std::string &value);
int SimpleDBDelete(const std::string &host, const std::string &value);
int SimpleDBUpdate(const std::string &host, const nlohmann::json &record);

}  // namespace simplified