#include "Server.hpp"

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
	if (port < 1024 || port > 65535) {
		std::cerr << "Port must be between 1024 and 65535" << std::endl;
		return -1;
	}
	
	//AF_INET = IPv4, SOCK_STREAM = TCP
	// point de communication (comme une ligne telephonique)
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);
	
	// init struct address
	struct sockaddr_in addr;
	addr.sin_family = AF_INET; // IPv4
	addr.sin_addr.s_addr = INADDR_ANY; // Accepte toutes les adresses IP
	addr.sin_port = htons(port); // Convert port to network byte order

	// identifier le socket (comme un numero de telephone)
	bind(socketFd, (struct sockaddr *)&addr, sizeof(addr));
	std::cout << "Server started on port " << port << std::endl;
	// ecouter les connexions entrantes (attends les appels)
	listen(socketFd, 5);
	std::cout << "Waiting for connections..." << std::endl;

	//epoll boucle = ecouter les events sur le socket (gere les appels entrants)
	int epollFd = epoll_create1(0);
	if (epollFd == -1) {
		std::cerr << "Failed to create epoll file descriptor" << std::endl;
		return -1;
	}
	while(1)
	{
		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = socketFd;

		static bool added = false;
		if (!added) {
			if (epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &event) == -1) {
				std::cerr << "Failed to add file descriptor to epoll" << std::endl;
				return -1;
			}
			added = true;
		}

		struct epoll_event events[10];
		int nfds = epoll_wait(epollFd, events, 10, -1);
		if (nfds == -1) {
			std::cerr << "Epoll wait failed" << std::endl;
			return -1;
		}

		for (int i = 0; i < nfds; ++i) {
			if (events[i].data.fd == socketFd) {
				// decroche le telephone
				int clientFd = accept(socketFd, NULL, NULL);
				if (clientFd == -1) {
					std::cerr << "Failed to accept connection" << std::endl;
					continue;
				}
				std::cout << "Client connected" << std::endl;
				// Add the new client to epoll
				// voir recv() et send()
			}
		}
	}
	close(socketFd);
	return 0;
}