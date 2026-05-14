#pragma once

#include "simpledb/core.hpp"

namespace simplified {

class Client {
 public:
  Client();
  ~Client();
  bool isConnected() const { return connected_; }
  void connect(const char *address, const char *port);
  void disconnect();
  int send(const std::string &message);
  int recv(std::string &response);

 private:
  bool connected_;
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace simplified