#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>

int stoi(std::string str)
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
	if (port == -1) {
		std::cerr << "Invalid port number" << std::endl;
		return -1;
	}
	int addr = socket(AF_INET, SOCK_STREAM, 0);
	bind(0, (struct sockaddr *)&addr, sizeof(addr));
	listen(addr, 5);
	std::cout << "Server started on port " << port << std::endl;
	std::cout << "Waiting for connections..." << std::endl;
	int client = accept(addr, NULL, NULL);
	if (client == -1) {
		std::cerr << "Error accepting connection" << std::endl;
		return -1;
	}
	std::cout << "Client connected" << std::endl;
	accept(addr, NULL, NULL);
	return 0;
}