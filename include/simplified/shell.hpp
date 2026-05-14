#pragma once

#include "simpledb/core.hpp"
#include "simplified/client.hpp"
#include <iostream>
#include <string>

namespace simplified {

class Shell {
 public:
  Shell();
  ~Shell();
  void start();

 private:
  void prompt();
  Client client_;
};

}  // namespace simplified