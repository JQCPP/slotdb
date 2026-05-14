#include "simplified/shell.hpp"
#include "simplified/client.hpp"
#include "common/cmdline.hpp"

#include <boost/algorithm/string.hpp>

namespace simplified {

static bool gQuit = false;

Shell::Shell() {}

Shell::~Shell() {}

void Shell::prompt() {
  std::cout << "Shell > " << std::flush;
  std::string buffer;
  std::getline(std::cin, buffer);

  if (buffer.empty()) return;

  // 分割命令
  std::vector<std::string> fields;
  boost::algorithm::split(fields, buffer, boost::is_any_of(" \t"),
                          boost::algorithm::token_compress_on);

  if (fields.empty()) return;

  std::string cmd = fields[0];

  // 处理quit命令
  if (cmd == "quit" || cmd == "exit") {
    gQuit = true;
    client_.disconnect();
    return;
  }

  // 处理help命令
  if (cmd == "help") {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  insert {\"_id\":\"...\", ...}  - Insert a record" << std::endl;
    std::cout << "  query {\"_id\":\"...\"}           - Query a record" << std::endl;
    std::cout << "  delete {\"_id\":\"...\"}         - Delete a record" << std::endl;
    std::cout << "  help                            - Show this help" << std::endl;
    std::cout << "  quit/exit                       - Exit shell" << std::endl;
    return;
  }

  // 其他命令需要连接服务器
  if (!client_.isConnected()) {
    std::cout << "Connecting to server..." << std::endl;
    client_.connect("127.0.0.1", "27017");
    if (!client_.isConnected()) {
      std::cerr << "Failed to connect to server" << std::endl;
      return;
    }
  }

  // 构造JSON请求
  nlohmann::json request;
  request["cmd"] = cmd;

  // 尝试解析剩余部分为JSON
  std::string data_str = buffer.substr(cmd.size());
  data_str = CmdLine::trim(data_str);

  if (!data_str.empty()) {
    try {
      request["data"] = nlohmann::json::parse(data_str);
    } catch (...) {
      std::cerr << "Invalid JSON data: " << data_str << std::endl;
      return;
    }
  }

  // 发送请求
  std::string message = request.dump();
  if (client_.send(message) != OK) {
    std::cerr << "Failed to send request" << std::endl;
    return;
  }

  // 接收响应
  std::string response;
  if (client_.recv(response) == OK) {
    std::cout << response << std::endl;
  } else {
    std::cerr << "Failed to receive response" << std::endl;
  }
}

void Shell::start() {
  std::cout << "Welcome to SimpleDB Shell!" << std::endl;
  std::cout << "Type 'help' for help, Ctrl+c or 'quit' to exit" << std::endl;
  while (!gQuit) {
    prompt();
  }
}

}  // namespace simplified
