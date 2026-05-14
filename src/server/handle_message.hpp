#pragma once

#include "simpledb/core.hpp"
#include "command/command.hpp"

namespace simplified {

void HandleMessage(const char *input, size_t *len, bool *disconnect,
                  std::vector<uint8_t> *output);

}  // namespace simplified