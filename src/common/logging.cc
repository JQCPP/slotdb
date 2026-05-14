#include "common/logging.hpp"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

void initLogEnvironment(const char *log_file) {
  boost::log::add_common_attributes();
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

  boost::log::add_console_log(std::cout);
  if (log_file != nullptr) {
    boost::log::add_file_log(log_file);
  }
}

void DB_LOG(int level, const std::string &msg) {
  using namespace boost::log::trivial;
  switch (level) {
    case 0:  // OK
      BOOST_LOG_TRIVIAL(info) << msg;
      break;
    case -1:  // COMMON_ERROR
      BOOST_LOG_TRIVIAL(error) << msg;
      break;
    default:
      BOOST_LOG_TRIVIAL(debug) << msg;
      break;
  }
}