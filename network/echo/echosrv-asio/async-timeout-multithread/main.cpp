#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#include <echosrv.hpp>

int main(int argc, char *argv[]) {
	try {
		if (argc != 2) {
			std::cerr << "Usage: " << argv[0] << " <port>\n";
			return 1;
		}

		boost::asio::io_context io_context;
		boost::asio::io_service::work *work;
		boost::thread_group threads;

		Server s(io_context, std::atoi(argv[1]));

		int cores_number = boost::thread::hardware_concurrency();
		for (std::size_t i = 0; i < cores_number; ++i) {
			threads.create_thread(
			    boost::bind(&boost::asio::io_context::run, &io_context));
		}

		boost::this_thread::sleep(boost::posix_time::seconds(120));
		io_context.run();
	} catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
