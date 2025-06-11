#include "Server.hpp"

volatile sig_atomic_t g_sig = 0;

static void handle_sigint(int signal)
{
	(void)signal;
	g_sig = 1;
}

Server::Server()
{}

Server::Server(Server const &src)
{
	*this = src;
}

Server::~Server()
{}

static bool starts_with(const std::string& str, const std::string& prefix) {
	return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

Server &Server::operator=(Server const &rhs)
{
	(void)rhs;
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
	signal(SIGINT, handle_sigint);

	while(1)
	{
		struct epoll_event events[10];
		int nfds = epoll_wait(_epollFd, events, 10, -1);

		if (g_sig)
		{
			std::cerr << YELLOW << "SIGINT detected, servers shutting down..." << RESET << std::endl;
			break;
		}

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

void Server::shutdown()
{
	std::map<int, Client*>::iterator it = _clientMap.begin();
	while (it != _clientMap.end())
	{
		close(it->first);
		delete it->second;
		it++;
	}
	std::cout << GREEN << "Server shut down. Thank you for having chosen our service. Have a nice day~" << RESET << std::endl;
}

void Server::handleNewConnection()
{
	int clientFd = accept(_serverFd, NULL, NULL);
	if (clientFd < 0) {
		std::cerr << RED << "Failed to accept new connection" << RESET << std::endl;
		return;
	}

	struct epoll_event clientEvent;
	clientEvent.events = EPOLLIN;
	clientEvent.data.fd = clientFd;

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &clientEvent) == -1) {
		std::cerr << RED << "Failed to add client to epoll" << RESET << std::endl;
		close(clientFd);
		return;
	}

	_clientMap[clientFd] = new Client(clientFd);

	std::cout << GREEN << "Client connected" << RESET << std::endl;
}

void Server::handleClientMessage(int fd)
{
	char buffer[1024];
	ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		if (bytes == 0)
			std::cout << YELLOW << "Client disconnected " << RESET << std::endl;
		else
			std::cerr << "Error receiving data" << std::endl;
		deleteClient(fd);
		return;
	}

	std::string data(buffer, bytes);
	std::stringstream ss(data);
	std::string line;

	std::cout << MAGENTA << "A Request Recieved:" << RESET << std::endl;
	std::cout << MAGENTA << data << RESET << std::endl;
	while (std::getline(ss, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		std::cout << "Received: " << line << std::endl;
		line[bytes - 1] = '\0';
		if (starts_with(line, "CONNECT ") || starts_with(line, "JOIN :"))
			continue;
		else if (starts_with(line, "PRIVMSG "))
			handlePrivateMessage(line, fd);
		else if (starts_with(line, "JOIN "))
			handleJoinChannel(line, fd);
		else if (starts_with(line, "PING "))
			send (fd, "PONG\r\n", 6, 0);
		else if (starts_with(line, "CAP LS"))
		{
			std::string capReply = ":irc.42.local CAP * LS :\r\n";
			send(fd, capReply.c_str(), capReply.length(), 0);
		}
		else if (starts_with(line, "PASS "))
			handlePassword(line, fd);
		else if (starts_with(line, "NICK "))
			handleNickname(line, fd);
		else if (starts_with(line, "USER "))
			handleUser(line, fd);
		else if (_clientMap[fd]->getIsRegistered())
		{
			std::cout << "Message sent" << std::endl;
			std::map<int, Client*>::iterator it = _clientMap.begin();
			while (it != _clientMap.end())
			{
				if (it->first != fd && it->second->getIsRegistered())
					send(it->first, line.c_str(), line.size(), 0);
				it++;
			}
		}
		else
		{
			std::cerr << RED << "Client not registered" << RESET << std::endl;
			send(fd, "You must register first\n", 24, 0);
		}
	}
}

void Server::deleteClient(int fd)
{
	delete _clientMap[fd];
	close(fd);
	_clientMap.erase(fd);
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
}

void Server::handlePrivateMessage(const std::string& line, int fd)
{
	std::cout << "Here ! #1" << std::endl;
	std::string chamess = line.substr(8);
	if (chamess.empty()) {
		send(fd, "You must specify a message to send\n", 36, 0);
		return;
	}
	size_t chan_start = chamess.find('#') + 1;
	size_t space = chamess.find(' ', chan_start);
	if (chan_start == std::string::npos || space == std::string::npos) return;
	std::string channel = chamess.substr(chan_start, space - chan_start);
	if (channel.empty()) {
		send(fd, "You must specify a channel to send messages to\n", 51, 0);
		return;
	}
	std::string message = chamess.substr(chamess.find(':') + 1);
	if (message.empty()) {
		send(fd, "You must specify a message to send\n", 36, 0);
		return;
	}
	std::cout << "Here ! #2" << std::endl;
	std::cout << _channels[channel].members.size() << std::endl;
	std::cout << channel << std::endl;
	for (std::set<int>::iterator it = _channels[channel].members.begin(); it != _channels[channel].members.end(); ++it)
	{
		std::cout << "Here ! #3" << std::endl;
		if (*it != fd)
		{
			std::cout << "Here ! #4" << std::endl;
			std::string msg = ":" + _clientMap[fd]->getNickname() + "!" + _clientMap[fd]->getUser() + "@localhost PRIVMSG #" + channel + " :" + message + "\r\n";
			std::cout << msg << std::endl;
			if (send(*it, msg.c_str(), msg.size(), 0) < 0)
				std::cerr << "Failed to send message to client " << *it << std::endl;
			else
				std::cout << "Message sent to client " << *it << std::endl;
		}
	}
}


void Server::handleJoinChannel(const std::string& line, int fd)
{
	std::string channel(line.c_str() + 6);
	std::cout << channel << std::endl;
	if (channel.empty()) {
		send(fd, "You must specify a channel to join\n", 36, 0);
		return;
	}
	if (_channels.find(channel) == _channels.end())
	{
		_channels[channel] = t_channel();
		_channels[channel].name = channel;
		_channels[channel].members.insert(fd);
		_channels[channel].operators.insert(fd);
		std::cout << "Channel " << channel << " created" << std::endl;
	}
	else
	{
		_channels[channel].members.insert(fd);
		std::cout << _channels[channel].members.size() << std::endl;
		std::cout << "Client " << fd << " joined channel " << channel << std::endl;
	}
}

void Server::handlePassword(const std::string& line, int fd)
{
	std::string password(line.c_str() + 5);
	if (password == _password) {
		std::cout << GREEN << "Password accepted" << RESET << std::endl;
		_clientMap[fd]->setIsRegistered(true);
		if (!_clientMap[fd]->getNickname().empty() && !_clientMap[fd]->getUsername().empty() && _clientMap[fd]->getIsRegistered())
		{
			std::string tmp = ":localhost 001 " + _clientMap[fd]->getNickname() + " :Welcome to the 42 IRC Server\r\n";
			send(fd, tmp.c_str(), tmp.size(), 0);
			tmp = ":localhost 002 " + _clientMap[fd]->getNickname() + " :Your host is localhost, running version 0.1\r\n";
			send(fd, tmp.c_str(), tmp.size(), 0);
			tmp = ":localhost 003 " + _clientMap[fd]->getNickname() + " :This server was created just now\r\n";
			send(fd, tmp.c_str(), tmp.size(), 0);
			tmp = ":localhost 004 " + _clientMap[fd]->getNickname() + " :localhost 0.1\r\n";
			send(fd, tmp.c_str(), tmp.size(), 0);
		}
	}
	else
	{
		std::cerr << RED << "Incorrect password" << RESET << std::endl;
		send(fd, "Incorrect password\n", 20, 0);
	}
}

void Server::handleNickname(const std::string& line, int fd)
{
	std::string nickname(line.c_str() + 5);
	_clientMap[fd]->setNickname(nickname);
	if (!_clientMap[fd]->getNickname().empty() && !_clientMap[fd]->getUsername().empty() && _clientMap[fd]->getIsRegistered())
	{
		std::string tmp = ":localhost 001 " + _clientMap[fd]->getNickname() + " :Welcome to the 42 IRC Server\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 002 " + _clientMap[fd]->getNickname() + " :Your host is localhost, running version 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 003 " + _clientMap[fd]->getNickname() + " :This server was created just now\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 004 " + _clientMap[fd]->getNickname() + " :localhost 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
	}
}

void Server::handleUser(const std::string& line, int fd)
{
	std::string username(line.c_str() + 5);
	std::string user = username.substr(0, username.find(' '));
	_clientMap[fd]->setUsername(username);
	_clientMap[fd]->setUser(user);
	if (!_clientMap[fd]->getNickname().empty() && !_clientMap[fd]->getUsername().empty() && _clientMap[fd]->getIsRegistered())
	{
		std::string tmp = ":localhost 001 " + _clientMap[fd]->getNickname() + " :Welcome to the 42 IRC Server\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 002 " + _clientMap[fd]->getNickname() + " :Your host is localhost, running version 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 003 " + _clientMap[fd]->getNickname() + " :This server was created just now\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 004 " + _clientMap[fd]->getNickname() + " :localhost 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
	}
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
