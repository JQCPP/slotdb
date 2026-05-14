#include "common/message.hpp"

namespace simplified {

std::vector<uint8_t> JsonToMsgpack(const nlohmann::json &json) {
  // json to string, then string to vector
  std::string str = json.dump();
  return std::vector<uint8_t>(str.begin(), str.end());
}

nlohmann::json MsgpackToJson(const char *data) {
  return nlohmann::json::parse(data);
}

}  // namespace simplified