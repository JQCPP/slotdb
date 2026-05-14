#pragma once

#include "simpledb/core.hpp"

namespace simplified {

class Options {
 public:
  Options();
  ~Options();
  void ReadCmd(int argc, char *argv[]);

  int port() const { return port_; }
  int maxPool() const { return max_pool_; }
  const std::string &dataPath() const { return data_path_; }
  const std::string &logFilePath() const { return log_file_path_; }

 private:
  int port_;
  int max_pool_;
  std::string data_path_;
  std::string log_file_path_;
};

}  // namespace simplified