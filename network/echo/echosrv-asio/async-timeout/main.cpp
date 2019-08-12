#include <boost/asio.hpp>

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

		Server s(io_context, std::atoi(argv[1]));

		io_context.run();
	} catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
