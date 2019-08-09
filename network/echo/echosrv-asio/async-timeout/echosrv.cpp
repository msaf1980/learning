#include <boost/asio.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

using boost::asio::steady_timer;
using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session> {
  public:
	session(tcp::socket socket) : socket_(std::move(socket)) {}

	void start() { do_read(); }

	void stop() {
		boost::system::error_code ignored_error;
		socket_.close(ignored_error);
		deadline_.cancel();
	}

  private:
	bool stopped() const { return !socket_.is_open(); }

	void check_deadline(steady_timer &deadline) {
		auto self(shared_from_this());
		deadline.async_wait([this, self, &deadline](
		                        const boost::system::error_code & /*error*/) {
			// Check if the session was stopped while the operation was pending.
			if (stopped())
				return;

			// Check whether the deadline has passed.
			if (deadline.expiry() <= steady_timer::clock_type::now()) {
				// The deadline has passed. Stop the session. The other actors
				// will terminate as soon as possible.
				stop();
			} else {
				// Put the actor back to sleep.
				check_deadline(deadline);
			}
		});
	}

	void do_read() {
		auto self(shared_from_this());
		deadline_.expires_after(std::chrono::seconds(10));
		socket_.async_read_some(
		    boost::asio::buffer(data_, max_length),
		    [this, self](boost::system::error_code ec, std::size_t length) {
			    if (ec) {
				    std::cerr << ec.message() << "\n";
				    stop();
			    } else {
				    if (strncmp(data_, "quit\r\n", length) == 0) {
					    stop();
				    } else {
					    do_write(length);
				    }
			    }
		    });
		check_deadline(deadline_);
	}

	void do_write(std::size_t length) {
		auto self(shared_from_this());
		deadline_.expires_after(std::chrono::seconds(2));
		boost::asio::async_write(
		    socket_, boost::asio::buffer(data_, length),
		    [this, self](boost::system::error_code ec, std::size_t /*length*/) {
			    if (ec) {
				    std::cerr << ec.message() << "\n";
				    stop();
			    } else {
				    do_read();
			    }
		    });
		check_deadline(deadline_);
	}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

	steady_timer deadline_{socket_.get_executor().context()};
};

class server {
  public:
	server(boost::asio::io_context &io_context, short port)
	    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
		do_accept();
	}

  private:
	void do_accept() {
		acceptor_.async_accept(
		    [this](boost::system::error_code ec, tcp::socket socket) {
			    if (!ec) {
				    std::make_shared<session>(std::move(socket))->start();
			    }

			    do_accept();
		    });
	}

	tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
	try {
		if (argc != 2) {
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
			return 1;
		}

		boost::asio::io_context io_context;

		server s(io_context, std::atoi(argv[1]));

		io_context.run();
	} catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
