#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#include <signal.h>

#include <echosrv.hpp>

int running = 1;

void handler(const boost::system::error_code &error, int signal_number) {
	// Not safe to use stream here..a
	if (signal_number == SIGKILL || signal_number == SIGINT) {
		std::cout << "Stopping" << std::endl;
		running = 0;
	}
}

int main(int argc, char *argv[]) {
	try {
		if (argc != 2) {
			std::cerr << "Usage: " << argv[0] << " <port>\n";
			return 1;
		}

		boost::asio::io_context        io_context;
		boost::asio::io_service::work *work;
		boost::thread_group            threads;

		// Wait for signals indicating time to shut down.
		boost::asio::signal_set signals(io_context);
		signals.add(SIGINT);
		signals.add(SIGTERM);

		signals.async_wait(handler);

		Server s(io_context, std::atoi(argv[1]));

		int cores_number = boost::thread::hardware_concurrency();
		for (std::size_t i = 0; i < cores_number; ++i) {
			threads.create_thread(
			    boost::bind(&boost::asio::io_context::run, &io_context));
		}

		while (running) {
			boost::this_thread::sleep(boost::posix_time::milliseconds(500));
		}
		io_context.stop();
		threads.join_all();
	} catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
