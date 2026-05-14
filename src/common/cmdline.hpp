#pragma once

#include "simpledb/core.hpp"

namespace simplified {

class CmdLine {
 public:
  static std::vector<std::string> split(const std::string &str);
  static std::string trim(const std::string &str);
};

}  // namespace simplified