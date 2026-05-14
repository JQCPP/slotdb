#include "command/icommand.hpp"

#include "pmd/pmd.hpp"

namespace simplified {

int InsertCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  auto dms = PmdManager::getPmdManager().getDmsFile();
  auto bucket = PmdManager::getPmdManager().getBucketManager();
  DmsRecordID rid;
  auto rc = dms->insert(input, output, rid);
  if (rc != OK) return rc;
  return bucket->createIndex(output, rid);
}

int QueryCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  auto bucket = PmdManager::getPmdManager().getBucketManager();
  DmsRecordID rid;
  auto rc = bucket->findIndex(input, rid);
  if (rc != OK) return rc;
  auto dms = PmdManager::getPmdManager().getDmsFile();
  return dms->find(rid, output);
}

int DeleteCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  auto bucket = PmdManager::getPmdManager().getBucketManager();
  DmsRecordID rid;
  auto rc = bucket->findIndex(input, rid);
  if (rc != OK) return rc;
  auto dms = PmdManager::getPmdManager().getDmsFile();
  rc = dms->remove(rid);
  if (rc != OK) return rc;
  bucket->removeIndex(input, rid);
  return OK;
}

int ConnectCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  output["status"] = "connected";
  return OK;
}

int HelpCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  output["commands"] = {"insert", "query", "delete", "help", "quit"};
  return OK;
}

int TestCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  output["result"] = "test ok";
  return OK;
}

int QuitCommand::execute(const nlohmann::json &input, nlohmann::json &output) {
  output["status"] = "quit";
  return OK;
}

}  // namespace simplified