#include "common/hash_code.hpp"

uint32_t HashCode(const char *str, size_t len) {
  uint32_t hash = 5381;
  for (size_t i = 0; i < len; ++i) {
    hash = ((hash << 5) + hash) + str[i];
  }
  return hash;
}