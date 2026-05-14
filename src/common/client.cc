#include "simplified/client.hpp"

#include <boost/asio.hpp>

namespace simplified {

class Client::Impl {
 public:
  boost::asio::io_service io_service_;
  std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
  boost::system::error_code error_;
  boost::array<char, 4096> buf_;
};

Client::Client() : connected_(false), impl_(new Impl) {}

Client::~Client() {
  disconnect();
}

void Client::connect(const char *address, const char *port) {
  try {
    boost::asio::ip::tcp::resolver resolver(impl_->io_service_);
    boost::asio::ip::tcp::resolver::query query(address, port);
    auto endpoint_iterator = resolver.resolve(query);
    impl_->socket_ = std::make_shared<boost::asio::ip::tcp::socket>(
        impl_->io_service_);
    boost::asio::connect(*impl_->socket_, endpoint_iterator);
    connected_ = true;
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

void Client::disconnect() {
  if (impl_->socket_) {
    boost::system::error_code ec;
    impl_->socket_->shutdown(
        boost::asio::ip::tcp::socket::shutdown_both, ec);
  }
  connected_ = false;
}

int Client::send(const std::string &message) {
  if (!connected_) return ErrSockNotConnect;
  try {
    boost::asio::write(*impl_->socket_,
                       boost::asio::buffer(message));
  } catch (std::exception &e) {
    return ErrSend;
  }
  return OK;
}

int Client::recv(std::string &response) {
  if (!connected_) return ErrSockNotConnect;
  auto len = impl_->socket_->read_some(
      boost::asio::buffer(impl_->buf_), impl_->error_);
  printf("[%s] %s\n", __FUNCTION__, response.c_str());
  printf(impl_->buf_.data(), len);
  response.assign(impl_->buf_.data(), len);
  return OK;
}

}  // namespace simplified