#include "Server.hpp"

static int stoi(const std::string& str)
{
	std::stringstream s(str);
	int i;

	if (!(s >> i) || !s.eof())
		return -1;
	return i;
}

int main(int ac, char **av) {
	if (ac != 3) {
		std::cerr << "Usage: " << av[0] << " <port> <password>" << std::endl;
		return -1;
	}
	int port = stoi(av[1]);
	if (port < 1024 || port > 65535) {
		std::cerr << "Port must be between 1024 and 65535" << std::endl;
		return -1;
	}
	try {
		Server server;
		server.init(port, av[2]);
		server.run();
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}
	return 0;
}
