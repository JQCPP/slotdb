#include "server/handle_message.hpp"

#include <boost/algorithm/string.hpp>
#include "common/message.hpp"
#include "common/logging.hpp"

namespace simplified {

void HandleMessage(const char *input, size_t *len, bool *disconnect,
                 std::vector<uint8_t> *output) {
  *disconnect = false;
  if (*len == 0) {
    output->clear();
    return;
  }

  // parse json request
  std::string input_str(input, *len);
  nlohmann::json input_json;
  try {
    input_json = nlohmann::json::parse(input_str);
  } catch (std::exception &e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to parse json: " << e.what();
    nlohmann::json err = {{"error", e.what()}};
    auto reply = err.dump();
    output->assign(reply.begin(), reply.end());
    *len = output->size();
    return;
  }

  // get command
  std::string cmd;
  try {
    cmd = input_json["cmd"];
  } catch (std::exception &e) {
    nlohmann::json err = {{"error", "missing cmd field"}};
    auto reply = err.dump();
    output->assign(reply.begin(), reply.end());
    *len = output->size();
    return;
  }
  // split command by space
  std::vector<std::string> fields;
  boost::algorithm::split(fields, cmd, boost::is_any_of(" "));

  // get command processor
  Command command;
  auto icmd = command.commandProcesser(fields);
  // execute command
  nlohmann::json output_json;
  if (icmd) {
    nlohmann::json data;
    if (input_json.contains("data")) {
      data = input_json["data"];
    }
    //execute command and get result code
    auto rc = icmd->execute(data, output_json);
    output_json["rc"] = rc;
  } else {
    output_json["error"] = "unknown command";
    output_json["rc"] = ErrCommand;
  }

  // check if command is quit
  if (cmd == "quit") {
    *disconnect = true;
  }

  // convert json to msgpack and send back
  auto reply = output_json.dump();
  output->assign(reply.begin(), reply.end());
  *len = output->size();
}

}  // namespace simplified