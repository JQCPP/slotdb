#include "common/cmdline.hpp"

#include <boost/algorithm/string.hpp>

namespace simplified {

std::vector<std::string> CmdLine::split(const std::string &str) {
  std::vector<std::string> result;
  boost::algorithm::split(result, str, boost::is_any_of(" \t\n"),
                        boost::token_compress_on);
  return result;
}

std::string CmdLine::trim(const std::string &str) {
  std::string result = str;
  boost::algorithm::trim(result);
  return result;
}

}  // namespace simplified