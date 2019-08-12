#ifndef _ECHOSRV_HPP_
#define _ECHOSRV_HPP_

#include <boost/asio.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

using boost::asio::steady_timer;
using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
  public:
	Session(tcp::socket socket);

	void start();
	void stop();

  private:
	bool stopped() const;
	void check_deadline(steady_timer &deadline);
	void do_read();
	void do_write(std::size_t length);

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

	steady_timer deadline_{socket_.get_executor().context()};
};

class Server {
  public:
	Server(boost::asio::io_context &io_context, short port);

  private:
	void do_accept();

	tcp::acceptor acceptor_;
};

#endif /* _ECHOSRV_HPP_ */
