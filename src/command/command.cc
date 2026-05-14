#include "command/command.hpp"

#include "command/icommand.hpp"

namespace simplified {

Command::Command() {
  //insert command
  cmd_map_[kCommandInsert] = new InsertCommand();
  //connect command
  cmd_map_[kCommandConnect] = new ConnectCommand();
  //query command
  cmd_map_[kCommandQuery] = new QueryCommand();
  //delete command
  cmd_map_[kCommandDelete] = new DeleteCommand();
  //help command
  cmd_map_[kCommandHelp] = new HelpCommand();
  //test command
  cmd_map_[kCommandTest] = new TestCommand();
}

Command::~Command() {
  for (auto &pair : cmd_map_) {
    delete pair.second;
  }
}

ICommand *Command::commandProcesser(const std::vector<std::string> &text) {
  //first token is looked upon as command
  auto cmd = text[0];
  auto result = cmd_map_.find(cmd);
  if (result != cmd_map_.end()) {
    return result->second;
  }
  return nullptr;
}

}  // namespace simplified