#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#include <echosrv.hpp>

using boost::asio::steady_timer;
using boost::asio::ip::tcp;

//######################################################
// Session class
Session::Session(tcp::socket socket) : socket_(std::move(socket)) {}

void Session::start() { do_read(); }

void Session::stop() {
	boost::system::error_code ignored_error;
	socket_.close(ignored_error);
	deadline_.cancel();
}

bool Session::stopped() const { return !socket_.is_open(); }

void Session::check_deadline(steady_timer &deadline) {
	auto self(shared_from_this());
	deadline.async_wait(
	    [this, self, &deadline](const boost::system::error_code & /*error*/) {
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

void Session::do_read() {
	auto self(shared_from_this());
	deadline_.expires_after(std::chrono::seconds(10));
	socket_.async_read_some(
	    boost::asio::buffer(data_, max_length),
	    [this, self](boost::system::error_code ec, std::size_t length) {
		    if (ec) {
			    //std::cerr << ec.message() << "\n";
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

void Session::do_write(std::size_t length) {
	auto self(shared_from_this());
	deadline_.expires_after(std::chrono::seconds(2));
	boost::asio::async_write(
	    socket_, boost::asio::buffer(data_, length),
	    [this, self](boost::system::error_code ec, std::size_t /*length*/) {
		    if (ec) {
			    //std::cerr << ec.message() << "\n";
			    stop();
		    } else {
			    do_read();
		    }
	    });
	check_deadline(deadline_);
}
// Session class
//######################################################

//######################################################
// Server class

Server::Server(boost::asio::io_context &io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
	do_accept();
}

void Server::do_accept() {
	acceptor_.async_accept(
	    [this](boost::system::error_code ec, tcp::socket socket) {
		    if (!ec) {
			    std::make_shared<Session>(std::move(socket))->start();
		    }

		    do_accept();
	    });
}
