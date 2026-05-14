#pragma once

#include "simpledb/core.hpp"
#include "command/icommand.hpp"

namespace simplified {

class Command {
 public:
  Command();
  ~Command();
  ICommand *commandProcesser(const std::vector<std::string> &text);

 private:
  std::unordered_map<std::string, ICommand *> cmd_map_;
};

}  // namespace simplified