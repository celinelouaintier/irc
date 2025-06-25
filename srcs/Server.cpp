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

std::string itos(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
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
			std::cout << GREEN << "Server shut down. Thank you for having chosen our service. Have a nice day~" << RESET << std::endl;
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
	
	if (data.find("\r\n") == std::string::npos) {
		_buffers[fd] += data; // Store the incomplete data for next time
		return; // Wait for more data
	}
	std::string s = _buffers[fd];

	_buffers.erase(fd);
	std::stringstream ss(s);
	std::string line;

	while (std::getline(ss, line)) {
    	if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		std::cout << MAGENTA << "\nA Request Received:" << RESET << std::endl;
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
			return;
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
			handleKickClient(line, fd);
		else if (starts_with(line, "PRIVMSG "))
			handlePrivateMessage(line, fd);
		else if (starts_with(line, "JOIN "))
			handleJoinChannel(line, fd);
		else if (starts_with(line, "TOPIC "))
			handleTopic(line, fd);
		else if (starts_with(line, "INVITE "))
			handleInvite(line, fd);
        else if (starts_with(line, "MODE "))
            handleMode(line, fd);
		else if (starts_with(line, "PING "))
			send (fd, "PONG\r\n", 6, 0);
		else if (starts_with(line, "PART "))
			handlePartChannel(line, fd);
		else if (starts_with(line, "QUIT"))
			handleQuit(line, fd);
	}
}

void Server::sendMessageToChannel(const std::string &channel, const std::string &msg, int fd, bool sendToSelf)
{
	for (std::set<int>::iterator it = _channels[channel].members.begin(); it != _channels[channel].members.end(); ++it)
	{
		if (!sendToSelf && *it == fd)
			continue;
		if (send(*it, msg.c_str(), msg.size(), 0) < 0) {
			std::cerr << RED << "Failed to send message to client " << *it << RESET << std::endl;
		} else {
			std::cout << "Message sent to client " << *it << " : " << _clients[*it].getNickname() << std::endl;
		}
	}
}

void Server::handlePassword(const std::string &line, int fd)
{
	std::string msg;
	if (_clients[fd].getIsRegistered())
	{
		msg = ":" + _clients[fd].getHostname() + " 462 " + _clients[fd].getNickname() + " :You may not reregister\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	std::string password(line.c_str() + 5);
	if (password == _password) {
		std::cout << "Password accepted" << std::endl;
		_clients[fd].setCorrectPassword(true);
		registerClientAndSendWelcome(fd);
	} else {
		std::cerr << RED << "Incorrect password" << RESET << std::endl;
		msg = ":" + _clients[fd].getHostname() + " 464 " + _clients[fd].getNickname() + " :Password incorrect\r\n";
		send(fd, msg.c_str(), msg.size(), 0);
	}
}

void Server::handleNickname(std::string &line, int fd)
{
	line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    std::string nickname = line.substr(line.find(' ') + 1);
	std::string msg;
    if (nickname.empty())
    {
        msg = ":" + _clients[fd].getHostname() + " 431 ";
        if (_clients[fd].getNickname().empty())
            msg += "*";
        else
            msg += _clients[fd].getNickname();
        msg += " :No nickname given\r\n";
        send(fd, msg.c_str(), msg.size(), 0);
        return;
    }

    if (nickname[0] == ':' || nickname[0] == '#')
    {
        msg = ":" + _clients[fd].getHostname() + " 432 ";
        if (_clients[fd].getNickname().empty())
            msg += "*";
        else
            msg += _clients[fd].getNickname();
        msg += " " + nickname + " :Erroneous nickname\r\n";
        send(fd, msg.c_str(), msg.size(), 0);
        return;
    }

    std::map<int, Client>::iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second.getNickname() == nickname && it->first != fd)
        {
            msg = ":" + _clients[fd].getHostname() + " 433 ";
            if (_clients[fd].getNickname().empty())
                msg += "*";
            else
                msg += _clients[fd].getNickname();
            msg += " " + nickname + " :Nickname is already in use\r\n";
            send(fd, msg.c_str(), msg.size(), 0);
            return;
        }
    }

    std::string oldNickname = _clients[fd].getNickname();
    if (_clients[fd].getIsRegistered() && !oldNickname.empty())
    {
        std::map<std::string, t_channel>::iterator channelIt;
        std::string nickChangeMsg = ":" + oldNickname + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " NICK :" + nickname + "\r\n";
        for (channelIt = _channels.begin(); channelIt != _channels.end(); ++channelIt)
        {
            const std::string& channelName = channelIt->first;
            const t_channel& channelData = channelIt->second;

            if (channelData.members.find(fd) != channelData.members.end())
                sendMessageToChannel(channelName, nickChangeMsg);
        }
		send(fd, nickChangeMsg.c_str(), nickChangeMsg.size(), 0);
    }

    _clients[fd].setNickname(nickname);
    std::cout << "Nickname for fd " << fd << " set to: " << nickname << std::endl;
    registerClientAndSendWelcome(fd);
}

void Server::handleUser(const std::string &line, int fd)
{
	if (_clients[fd].getIsRegistered())
	{
		std::string msg = ":" + _clients[fd].getHostname() + " 462 " + _clients[fd].getNickname() + " :You may not reregister\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	std::string username(line.c_str() + 5);
	std::string user = username.substr(0, username.find(' '));
	_clients[fd].setUsername(username);
	_clients[fd].setUser(user);
	registerClientAndSendWelcome(fd);
}

void Server::registerClientAndSendWelcome(int fd)
{
	if (!_clients[fd].getNickname().empty() && !_clients[fd].getUsername().empty() && _clients[fd].getCorrectPassword() &&!_clients[fd].getIsRegistered())
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

void Server::leaveChannel(const std::string &channel, int fd)
{
	if (_channels.find(channel) == _channels.end())
		return;

	_channels[channel].members.erase(fd);
	if (_channels[channel].operators.find(fd) != _channels[channel].operators.end())
		_channels[channel].operators.erase(fd);

	if (_channels[channel].members.empty()) {
		_channels.erase(channel);
		std::cout << "Channel " << channel << " deleted" << std::endl;
	} else
		std::cout << "Client " << fd << " left channel " << channel << std::endl;
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