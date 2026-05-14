#include "common/options.hpp"

#include <boost/program_options.hpp>

namespace simplified {

namespace po = boost::program_options;

Options::Options() : port_(27017), max_pool_(4), data_path_("./data"), log_file_path_("./log.cfg") {}

Options::~Options() {}

void Options::ReadCmd(int argc, char *argv[]) {
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>(&port_)->default_value(27017), "set port")
      ("pool,t", po::value<int>(&max_pool_)->default_value(4), "set thread pool size")
      ("data,d", po::value<std::string>(&data_path_)->default_value("./data"), "set data path")
      ("log,l", po::value<std::string>(&log_file_path_)->default_value("./log.cfg"), "set log file");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
}

}  // namespace simplified