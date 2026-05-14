#include "simplified/simpledb.hpp"

namespace simplified {

int SimpleDBInsert(const std::string &host, const nlohmann::json &record) {
  return OK;
}

std::string SimpleDBQuery(const std::string &host, const std::string &value) {
  return "";
}

int SimpleDBDelete(const std::string &host, const std::string &value) {
  return OK;
}

int SimpleDBUpdate(const std::string &host, const nlohmann::json &record) {
  return OK;
}

}  // namespace simplified