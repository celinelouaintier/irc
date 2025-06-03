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

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

Server &Server::operator=(Server const &rhs)
{
    if (this != &rhs)
	{
		_serverFd = rhs._serverFd;
		_epollFd = rhs._epollFd;
		_serverAddr = rhs._serverAddr;
		_clients = rhs._clients;
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
	_clients[clientFd] = Client(clientFd);
	std::cout << "Client connected" << std::endl;
}

void Server::handleClientMessage(int fd)
{
	char buffer[1024];
	ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
	std::string data(buffer, bytes);
	std::stringstream ss(data);
	std::string line;

	while (std::getline(ss, line)) {
    	if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		std::cout << "Received: " << line << std::endl;
		if (!bytes) {
			std::cout << "Client disconnected " << std::endl;
			deleteClient(fd);
		}
		else if (bytes < 0) {
			std::cerr << "Error receiving data" << std::endl;
			deleteClient(fd);
		}
		else if (starts_with(line, "CONNECT ") || starts_with(line, "JOIN :"))
			continue;
		else if (starts_with(line, "PASS ")) {
			line[bytes - 1] = '\0';
			std::string password(line.c_str() + 5);
			if (password == _password) {
				std::cout << "Password accepted" << std::endl;
				registerClientAndSendWelcome(fd);
			} else {
				std::cerr << "Incorrect password" << std::endl;
				send(fd, "Incorrect password\n", 20, 0);
			}
		}
		else if (starts_with(line, "NICK "))
		{
			line[bytes - 1] = '\0';
			std::string nickname(line.c_str() + 5);
			_clients[fd].setNickname(nickname);
			std::cout << "Nickname set to: " << nickname << std::endl;
			registerClientAndSendWelcome(fd);
		}
		else if (starts_with(line, "USER ")) {
			line[bytes - 1] = '\0';
			std::string username(line.c_str() + 5);
			std::string user = username.substr(0, username.find(' '));
			_clients[fd].setUsername(username);
			_clients[fd].setUser(user);
			std::cout << "Username set to: " << username << std::endl;
			registerClientAndSendWelcome(fd);
		}
		else if (!_clients[fd].getIsRegistered()) {
			std::cerr << "Client not registered" << std::endl;
			send(fd, "You must register first\n", 24, 0);
		}
		else if (starts_with(line, "PRIVMSG "))
		{
			line[bytes - 1] = '\0';
			std::string info = line.substr(8);
			if (info.empty()) {
				send(fd, "You must specify a message to send\n", 36, 0);
				continue;
			}
			std::string dest = info.substr(0, info.find(' '));
			if (dest.empty()) {
				send(fd, "You must specify a destination to send the message to\n", 51, 0);
				continue;
			}
			std::string message = info.substr(info.find(':') + 1);
			if (message.empty()) {
				send(fd, "You must specify a message to send\n", 36, 0);
				continue;
			}
			std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@localhost PRIVMSG " + dest + " :" + message + "\r\n";
			for (std::set<int>::iterator it = _channels[dest].members.begin(); it != _channels[dest].members.end(); ++it)
			{
				if (*it != fd)
				{
					if (send(*it, msg.c_str(), msg.size(), 0) < 0)
						std::cerr << "Failed to send message to client " << *it << std::endl;
					else
						std::cout << "Message sent to client " << *it << std::endl;
				}
			}
			for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
			{
				if (it->second.getNickname() == dest && it->second.getIsRegistered())
				{
					if (send(it->first, msg.c_str(), msg.size(), 0) < 0)
						std::cerr << "Failed to send message to client " << it->first << std::endl;
					else
						std::cout << "Message sent to client " << it->first << std::endl;
					break;
				}
			}
		}
		else if (starts_with(line, "JOIN ")) {
			line[bytes - 1] = '\0';
			std::string channel(line.c_str() + 5);
			if (channel.empty()) {
				send(fd, "You must specify a channel to join\n", 36, 0);
				continue;
			}
			if (_channels.find(channel) == _channels.end()) {
				_channels[channel] = t_channel();
				_channels[channel].name = channel;
				_channels[channel].members.insert(fd);
				_channels[channel].operators.insert(fd);
				std::cout << "Channel " << channel << " created" << std::endl;
			}
			else {
				_channels[channel].members.insert(fd);
				std::cout << "Client " << fd << " joined channel " << channel << std::endl;
			}
		}
		else if (starts_with(line, "PING ")) {
			send (fd, "PONG\r\n", 6, 0);
		}
		else if (starts_with(line, "CAP LS"))
		{
			std::string capReply = ":irc.42.local CAP * LS :\r\n";
			send(fd, capReply.c_str(), capReply.length(), 0);
		}
	}
}

void Server::registerClientAndSendWelcome(int fd)
{
	if (!_clients[fd].getNickname().empty() && !_clients[fd].getUsername().empty() && !_clients[fd].getIsRegistered())
	{
		_clients[fd].setIsRegistered(true);
		std::cout << "Client " << fd << " registered" << std::endl;
		std::string tmp = ":localhost 001 " + _clients[fd].getNickname() + " :Welcome to the 42 IRC Server\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 002 " + _clients[fd].getNickname() + " :Your host is localhost, running version 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 003 " + _clients[fd].getNickname() + " :This server was created just now\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":localhost 004 " + _clients[fd].getNickname() + " :localhost 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
	}
}

void Server::deleteClient(int fd)
{
	_clients.erase(fd);
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