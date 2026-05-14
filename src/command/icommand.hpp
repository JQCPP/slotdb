#pragma once

#include "simpledb/core.hpp"

namespace simplified {

class ICommand {
  public:
  virtual ~ICommand() = default;
  virtual int execute(const nlohmann::json &input, nlohmann::json &output) = 0;
};

class InsertCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

class QueryCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

class DeleteCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

class ConnectCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

class HelpCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

class TestCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

class QuitCommand : public ICommand {
  public:
  int execute(const nlohmann::json &input, nlohmann::json &output) override;
};

constexpr const char *kCommandInsert = "insert";
constexpr const char *kCommandConnect = "connect";
constexpr const char *kCommandQuery = "query";
constexpr const char *kCommandDelete = "delete";
constexpr const char *kCommandHelp = "help";
constexpr const char *kCommandTest = "test";
constexpr const char *kCommandSnapshot = "snapshot";
constexpr const char *kCommandShutdown = "shutdown";
constexpr const char *kCommandQuit = "quit";

}  // namespace simplified