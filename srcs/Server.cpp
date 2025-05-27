#include "Server.hpp"

Server::Server()
{

}

Server::Server(Server const &src)
{
    *this = src;
}

Server::~Server()
{

}

Server &Server::operator=(Server const &rhs)
{
    if (this != &rhs)
	{
		_serverFd = rhs._serverFd;
		_epollFd = rhs._epollFd;
		_serverAddr = rhs._serverAddr;
		_clientFds = rhs._clientFds;
		_password = rhs._password;
	}
    return *this;
}

void Server::init(int port, const std::string &password)
{
	_password = password;
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverFd < 0)
		throw CreateSocketException();

	_serverAddr.sin_family = AF_INET;
	_serverAddr.sin_addr.s_addr = INADDR_ANY;
	_serverAddr.sin_port = htons(port);

	if (bind(_serverFd, (struct sockaddr *)&_serverAddr, sizeof(_serverAddr)) < 0)
		throw BindingSocketException();
	std::cout << "Server started on port " << port << std::endl;
	if (listen(_serverFd, 5) < 0) 
		throw listeningSocketException();
	std::cout << "Waiting for connections..." << std::endl;

	_epollFd = epoll_create1(0);
	if (_epollFd == -1)
		throw CreateEpollException();
}

void Server::run()
{
	struct epoll_event serverEvent;
	serverEvent.events = EPOLLIN;
	serverEvent.data.fd = _serverFd;

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverFd, &serverEvent) == -1)
		throw CreateEpollException();

	while(1)
	{
		struct epoll_event events[10];
		int nfds = epoll_wait(_epollFd, events, 10, -1);
		if (nfds == -1)
			throw EpollWaitException();

		for (int i = 0; i < nfds; ++i) {
			int fd = events[i].data.fd;
			if (fd == _serverFd)
				handleNewConnection();
			else
				handleClientMessage(fd);
		}
	}
}

void Server::handleNewConnection()
{
	int clientFd = accept(_serverFd, NULL, NULL);
	if (clientFd < 0) {
		std::cerr << "Failed to accept new connection" << std::endl;
		return;
	}

	struct epoll_event clientEvent;
	clientEvent.events = EPOLLIN;
	clientEvent.data.fd = clientFd;

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &clientEvent) == -1) {
		std::cerr << "Failed to add client to epoll" << std::endl;
		close(clientFd);
		return;
	}

	_clientFds.push_back(clientFd);
	std::cout << "Client connected" << std::endl;
}

void Server::handleClientMessage(int fd)
{
	char buffer[1024];
	ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (!bytes) {
		std::cout << "Client disconnected " << std::endl;
		deleteClient(fd);
	}
	else if (bytes < 0) {
		std::cerr << "Error receiving data" << std::endl;
		deleteClient(fd);
	}
	else {
		buffer[bytes] = '\0';
		std::cout << "Message sent" << std::endl;
		for (size_t c = 0; c < _clientFds.size(); c++)
			if (_clientFds[c] != fd)
				send(_clientFds[c], buffer, bytes, 0);
	}
}

void Server::deleteClient(int fd)
{
	_clientFds.erase(std::remove(_clientFds.begin(), _clientFds.end(), fd), _clientFds.end());
	close(fd);
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
}

const char* Server::CreateSocketException::what() const throw()
{
	return "Creating socket failed";
}

const char* Server::BindingSocketException::what() const throw()
{
	return "Binding socket failed";
}

const char* Server::listeningSocketException::what() const throw()
{
	return "Listening on socket failed";
}

const char* Server::CreateEpollException::what() const throw()
{
	return "Failed to create epoll file descriptor";
}

const char* Server::EpollWaitException::what() const throw()
{
	return "Epoll wait failed";
}