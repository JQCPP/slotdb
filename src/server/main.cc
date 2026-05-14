#include "simpledb/core.hpp"
#include "common/options.hpp"
#include "pmd/pmd.hpp"
#include "common/logging.hpp"
#include "server/threadpool.hpp"
#include "server/handle_message.hpp"

#include <boost/asio.hpp>

std::atomic<bool> gQuit(false);

static int SignalHandler() {
  signal(SIGINT, SIG_IGN);
  return 0;
}

int main(int argc, char *argv[]) {
  simplified::Options option;
  option.ReadCmd(argc, argv);
  auto &pmd_manager = simplified::PmdManager::getPmdManager();

  SignalHandler();
  pmd_manager.init(option);
  initLogEnvironment(option.logFilePath().c_str());

  try {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::acceptor acceptor(
        io_service, boost::asio::ip::tcp::endpoint(
                      boost::asio::ip::tcp::v4(), option.port()));

    simplified::ThreadPool pool(option.maxPool());
    while (!gQuit) {
      auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service);
      acceptor.accept(*socket);
      BOOST_LOG_TRIVIAL(info) << "Accept a new connection from client";

      pool.execute([socket]() {
        bool disconnect = false;
        while (!disconnect) {
          boost::system::error_code err;
          boost::array<char, RECV_BUFFER_SIZE> buf;
          boost::system::error_code ignored_error;
          std::vector<uint8_t> reply_message(SEND_BUFFER_SIZE, 0);

          auto len = socket->read_some(boost::asio::buffer(buf), err);
          if (err) {
            DB_LOG(COMMON_ERROR, err.message());
            return;
          }
          simplified::HandleMessage(buf.data(), &len, &disconnect, &reply_message);
          boost::asio::write(*socket, boost::asio::buffer(reply_message, len),
                            boost::asio::transfer_all(), ignored_error);
        }
      });
    }
    BOOST_LOG_TRIVIAL(debug) << "Shutdown...";
  } catch (std::exception &e) {
    BOOST_LOG_TRIVIAL(error) << e.what();
  }

  return 0;
}