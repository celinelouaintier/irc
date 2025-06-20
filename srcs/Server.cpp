#include "Server.hpp"

volatile sig_atomic_t g_sig = 0;

static void handle_sigint(int signal)
{
	(void)signal;
	g_sig = 1;
}

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
	signal(SIGINT, handle_sigint);

	while(1)
	{
		struct epoll_event events[10];
		int nfds = epoll_wait(_epollFd, events, 10, -1);

		if (g_sig)
		{

			std::cerr << YELLOW << "\nSIGINT detected, servers shutting down..." << RESET << std::endl;
			break;
		}

		if (nfds == -1)
			throw EpollWaitException();

		for (int i = 0; i < nfds; ++i) {
			int fd = events[i].data.fd;
			if (fd == _serverFd)
				handleNewConnection();
			else
				handleCommand(fd);
		}
	}
}

void Server::shutdown()
{
	std::map<int, Client>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		deleteClient(it->first);
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
	_clients[clientFd] = Client(clientFd);
	std::cout << GREEN << "Client connected" << RESET << std::endl;
}

void Server::removeChannel(const std::string &channelName)
{
	std::map<std::string, t_channel>::iterator it = _channels.find(channelName);
	if (it != _channels.end())
	{
		_channels.erase(it);
		std::cout << "Channel " << channelName << " removed" << std::endl;
	}
	else
	{
		std::cerr << RED << "Channel " << channelName << " not found" << RESET << std::endl;
	}
}

void Server::handleCommand(int fd)
{
	char buffer[1024];
	ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
	std::string data(buffer, bytes);
	std::stringstream ss(data);
	std::string line;


	while (std::getline(ss, line)) {
    	if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		std::cout << MAGENTA << "A Request Received:" << RESET << std::endl;
		std::cout << MAGENTA << line << RESET << std::endl;
		if (!bytes) {
			std::cout << "Client disconnected " << std::endl;
			deleteClient(fd);
		}
		else if (bytes < 0) {
			std::cerr << RED << "Error receiving data" << RESET << std::endl;
			deleteClient(fd);
		}
		else if (starts_with(line, "CONNECT "))
		{
			_clients[fd].setHostname(line.substr(8, line.find(' ')));
			std::cout << _clients[fd].getHostname() << std::endl;
			std::cout << _clients[fd].getHostname().size() << std::endl;
		}
		else if (starts_with(line, "JOIN :"))
			continue;
		else if (starts_with(line, "PASS "))
			handlePassword(line, fd);
		else if (starts_with(line, "NICK "))
			handleNickname(line, fd);
		else if (starts_with(line, "USER "))
			handleUser(line, fd);
		else if (starts_with(line, "CAP LS"))
		{
			std::string capReply = ":" + _clients[fd].getHostname() + " CAP * LS :\r\n";
			send(fd, capReply.c_str(), capReply.length(), 0);
		}
		else if (!_clients[fd].getIsRegistered()) {
			std::cerr << RED << "Client not registered" << RESET << std::endl;
			send(fd, "You must register first\n", 24, 0);
		}
		else if (starts_with(line, "KICK "))
			handleKickClient(line, fd, bytes);
		else if (starts_with(line, "PRIVMSG "))
			handlePrivateMessage(line, fd);
		else if (starts_with(line, "JOIN "))
			handleJoinChannel(line, fd);
		else if (starts_with(line, "PING ")) {
			send (fd, "PONG\r\n", 6, 0);
		}
		else if (starts_with(line, "PART "))
		{	
			line[bytes - 1] = '\0';
			std::string info = line.substr(5);
			std::string message = "";
			if (info.empty()) {
				send(fd, "You must specify a channel and a nickname to kick\n", 50, 0);
				return;
			}
			std::string channel = info.substr(0, info.find(' '));
			if (_channels.find(channel) == _channels.end()) {
				send(fd, "Channel does not exist\n", 23, 0);
				return;
			}
			if (info.find(':') != std::string::npos)
				message = info.substr(info.find(':') + 1);
			std::cout << channel << std::endl;
			std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " PART " + channel + "\r\n";
			for (std::set<int>::iterator it = _channels[channel].members.begin(); it != _channels[channel].members.end(); ++it)
			{
				if (send(*it, msg.c_str(), msg.size(), 0) < 0)
					std::cerr << RED << "Failed to send message to client " << *it << RESET << std::endl;
				else
					std::cout << "Message sent to client " << *it << std::endl;
			}
			_channels[channel].members.erase(fd);
			if (_channels[channel].operators.find(fd) != _channels[channel].operators.end())
				_channels[channel].operators.erase(fd);
			_clients[fd].removeChannel(channel);
			// Remove the channel if there are no more members
			if (_channels[channel].members.empty())
				removeChannel(channel);
		}
		else if (starts_with(line, "QUIT "))
		{
			Client& client = _clients[fd];
			std::string msg = ":" + client.getNickname() + "!" + client.getUser() + "@" + client.getHostname() + " QUIT :" + line.substr(5) + "\r\n";
			std::vector<std::string> channelsToRemove;
			for (std::map<std::string, t_channel>::iterator it = _channels.begin(); it != _channels.end(); ++it)
			{
				if (it->second.members.find(fd) != it->second.members.end())
				{
					it->second.members.erase(fd);
					if (it->second.operators.find(fd) != it->second.operators.end())
						it->second.operators.erase(fd);
					std::cout << "Client " << fd << " removed from channel " << it->first << std::endl;
				}
				if (it->second.members.empty())
					channelsToRemove.push_back(it->first);
			}
			for (size_t i = 0; i < channelsToRemove.size(); ++i)
			{
				// Remove the channel from all clients' channel lists
				for (std::map<int, Client>::iterator cit = _clients.begin(); cit != _clients.end(); ++cit)
					cit->second.removeChannel(channelsToRemove[i]);
				removeChannel(channelsToRemove[i]);
			}
			deleteClient(fd);
		}
	}
}

void Server::handleKickClient(std::string& line, int fd, int bytes)
{
	line[bytes - 1] = '\0';
	std::string info = line.substr(5);
	if (info.empty()) {
		send(fd, "You must specify a channel and a nickname to kick\n", 50, 0);
		return;
	}
	std::string channel = info.substr(0, info.find(' '));
	if (_channels.find(channel) == _channels.end()) {
		send(fd, "Channel does not exist\n", 23, 0);
		return;
	}
	std::string nickname = info.substr(info.find(' ') + 1, info.find(':') - info.find(' ') - 2);
	std::cout << nickname << std::endl;
	if (nickname.empty()) {
		send(fd, "You must specify a nickname to kick\n", 37, 0);
		return;
	}
	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end()) {
		send(fd, "You are not an operator of this channel\n", 41, 0);
		return;
	}
	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " KICK " + channel + " :" + nickname + "\r\n";
	int kickfd = -1;
	for (std::set<int>::iterator it = _channels[channel].members.begin(); it != _channels[channel].members.end(); ++it)
	{
		send(*it, msg.c_str(), msg.size(), 0);
		if (_clients[*it].getNickname() == nickname)
			kickfd = *it;
	}
	if (kickfd == fd)
	{
		send(fd, "You cannot kick yourself\n", 25, 0);
		return;
	}
	if (kickfd > 0)
	{
		_channels[channel].members.erase(kickfd);
		if (_channels[channel].operators.find(kickfd) != _channels[channel].operators.end())
			_channels[channel].operators.erase(kickfd);
		std::cout << "Client " << kickfd << " kicked from channel " << channel << std::endl;
	}
}

void Server::handlePassword(const std::string& line, int fd)
{
	std::string password(line.c_str() + 5);
	if (password == _password) {
		std::cout << "Password accepted" << std::endl;
		registerClientAndSendWelcome(fd);
	} else {
		std::cerr << RED << "Incorrect password" << RESET << std::endl;
		send(fd, "Incorrect password\n", 20, 0);
	}
}

void Server::handleNickname(const std::string& line, int fd)
{
	std::string nickname(line.c_str() + 5);
	if (!_clients[fd].getNickname().empty())
	{
		std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " NICK " + nickname + "\r\n";
		send(fd, msg.c_str(), msg.size(), 0);
	}
	_clients[fd].setNickname(nickname);
	std::cout << "Nickname set to: " << nickname << std::endl;
	registerClientAndSendWelcome(fd);
}

void Server::handleUser(const std::string& line, int fd)
{
	std::string username(line.c_str() + 5);
	std::string user = username.substr(0, username.find(' '));
	_clients[fd].setUsername(username);
	_clients[fd].setUser(user);
	registerClientAndSendWelcome(fd);
}

void Server::handleJoinChannel(const std::string& line, int fd)
{
	Client& client = _clients[fd];
	std::string channel(line.c_str() + 5);
	if (channel.empty()) {
		send(fd, "You must specify a channel to join\n", 36, 0);
		return;
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
	client.addChannel(channel);
}

void Server::handlePrivateMessage(const std::string& line, int fd)
{
	std::string info = line.substr(8);
	if (info.empty()) {
		send(fd, "You must specify a message to send\n", 36, 0);
		return	;
	}
	std::string dest = info.substr(0, info.find(' '));
	if (dest.empty()) {
		send(fd, "You must specify a destination to send the message to\n", 51, 0);
		return;
	}
	std::string message = info.substr(info.find(':') + 1);
	if (message.empty()) {
		send(fd, "You must specify a message to send\n", 36, 0);
		return;
	}
	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " PRIVMSG " + dest + " :" + message + "\r\n";
	for (std::set<int>::iterator it = _channels[dest].members.begin(); it != _channels[dest].members.end(); ++it)
	{
		if (*it != fd)
		{
			if (send(*it, msg.c_str(), msg.size(), 0) < 0)
				std::cerr << RED << "Failed to send message to client " << *it << RESET << std::endl;
			else
				std::cout << "Message sent to client " << *it << std::endl;
		}
	}
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.getNickname() == dest && it->second.getIsRegistered())
		{
			if (send(it->first, msg.c_str(), msg.size(), 0) < 0)
				std::cerr << RED << "Failed to send message to client " << it->first << RESET << std::endl;
			else
				std::cout << "Message sent to client " << it->first << std::endl;
			break;
		}
	}
}

void Server::registerClientAndSendWelcome(int fd)
{
	if (!_clients[fd].getNickname().empty() && !_clients[fd].getUsername().empty() && !_clients[fd].getIsRegistered())
	{
		_clients[fd].setIsRegistered(true);
		std::cout << "Client " << fd << " registered" << std::endl;
		std::string tmp = ":" + _clients[fd].getHostname() + " 001 " + _clients[fd].getNickname() + " :Welcome to the 42 IRC Server\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":" + _clients[fd].getHostname() + " 002 " + _clients[fd].getNickname() + " :Your host is " + _clients[fd].getHostname() + ", running version 0.1\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":" + _clients[fd].getHostname() + " 003 " + _clients[fd].getNickname() + " :This server was created just now\r\n";
		send(fd, tmp.c_str(), tmp.size(), 0);
		tmp = ":" + _clients[fd].getHostname() + " 004 " + _clients[fd].getNickname() + " :" + _clients[fd].getHostname() + " 0.1\r\n";
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