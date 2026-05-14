#pragma once

#include "simpledb/core.hpp"

void initLogEnvironment(const char *log_file);
void DB_LOG(int level, const std::string &msg);