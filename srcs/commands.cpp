#include "Server.hpp"

// KICK command to kick a client from a channel
void Server::handleKickClient(std::string& line, int fd)
{
	std::string message = "";

	std::string info = line.substr(5);
	if (info.empty())
		return (void)send(fd, "You must specify a channel and a nickname to kick\n", 50, 0);

	std::string channel = info.substr(0, info.find(' '));
	if (_channels.find(channel) == _channels.end())
		return (void)send(fd, "Channel does not exist\n", 23, 0);

	std::string nickname = info.substr(info.find(' ') + 1, info.find(':') - info.find(' ') - 2);
	if (nickname.empty())
		return (void)send(fd, "You must specify a nickname to kick\n", 37, 0);
	if (_clients[fd].getNickname() == nickname)
		return (void)send(fd, "You cannot kick yourself\n", 25, 0);

	if (info.find(':') != std::string::npos)
		message = info.substr(info.find(':') + 1);

	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
		return (void)send(fd, "You are not an operator of this channel\n", 41, 0);

	int kickfd = -1;
	for (std::set<int>::iterator it = _channels[channel].members.begin(); it != _channels[channel].members.end(); ++it)
	{
		if (_clients[*it].getNickname() == nickname)
			kickfd = *it;
	}
	if (kickfd == -1)
		return (void)send(fd, "Client not found in channel\n", 29, 0);

	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " KICK " + channel + " " + nickname + " :" + message + "\r\n";
	sendMessageToChannel(channel, msg);

	if (kickfd > 0)
		leaveChannel(channel, kickfd);
}

// PRIVMSG command
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

// JOIN command to join a channel
void Server::handleJoinChannel(const std::string& line, int fd)
{
	Client& client = _clients[fd];
	std::string channel(line.c_str() + 5);

	if (channel.empty())
		return (void)send(fd, "You must specify a channel to join\n", 36, 0);
	if (_channels.find(channel) == _channels.end()) {
		_channels[channel] = t_channel();
		_channels[channel].name = channel;
		_channels[channel].members.insert(fd);
		_channels[channel].operators.insert(fd);
		std::cout << "Channel " << channel << " created" << std::endl;
	}
	else if (_channels[channel].isInviteOnly && _channels[channel].invitedUsers.find(client.getNickname()) == _channels[channel].invitedUsers.end())
		return (void)send(fd, "This channel is invite-only\n", 28, 0);
	else if (_channels[channel].members.find(fd) != _channels[channel].members.end())
		return (void)send(fd, "You are already a member of this channel\n", 42, 0);
	else {
		_channels[channel].members.insert(fd);
		std::cout << "Client " << fd << " joined channel " << channel << std::endl;
	}
	client.addChannel(channel);
}

void Server::handlePartChannel(const std::string& line, int fd)
{
	std::string message = "";
	std::string info = line.substr(5);
	if (info.empty()) 
		return (void)send(fd, "You must specify a channel to leave\n", 36, 0);

	std::string channel = info;
	if (info.find(':') != std::string::npos)
	{
		message = info.substr(info.find(':') + 1);
		channel = info.substr(0, info.find(' '));
	}
	if (_channels.find(channel) == _channels.end())
		return (void)send(fd, "Channel does not exist\n", 23, 0);
	if (_channels[channel].members.find(fd) == _channels[channel].members.end())
		return (void)send(fd, "You are not a member of this channel\n", 34, 0);

	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " PART " + channel + " :" + message + "\r\n";
	sendMessageToChannel(channel, msg);

	leaveChannel(channel, fd);
}


void Server::handleTopic(std::string &line, int fd)
{
	line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

	std::string info = line.substr(6);
	if (info.empty())
		return (void)send(fd, "You must specify a channel to set the topic for\n", 47, 0);

	std::string channel = info.substr(0, info.find(' '));
	if (_channels.find(channel) == _channels.end())
		return (void)send(fd, "Channel does not exist\n", 23, 0);
	if (_channels[channel].members.find(fd) == _channels[channel].members.end())
		return (void)send(fd, "You are not a member of this channel\n", 34, 0);
	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
		return (void)send(fd, "You are not an operator of this channel\n", 41, 0);

	std::string topic = line.substr(line.find(':') + 1);
	_channels[channel].topic = topic;

	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " TOPIC " + channel + " :" + topic + "\r\n";
	sendMessageToChannel(channel, msg);
}

void Server::handleInvite(std::string& line, int fd)
{
	std::string info = line.substr(7);
	if (info.empty())
		return (void)send(fd, "You must specify a channel and a nickname to invite\n", 53, 0);

	std::string nickname = info.substr(0, info.find(' '));
	if (nickname.empty())
		return (void)send(fd, "You must specify a nickname to invite\n", 38, 0);

	std::string channel = info.substr(info.find(' ') + 1);
	if (_channels.find(channel) == _channels.end())
		return (void)send(fd, "Channel does not exist\n", 23, 0);

	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
		return (void)send(fd, "You are not an operator of this channel\n", 41, 0);

	_channels[channel].invitedUsers.insert(nickname);
	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " INVITE " + nickname + " " + channel + "\r\n";
	send(fd, msg.c_str(), msg.size(), 0);

	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.getNickname() == nickname && it->second.getIsRegistered())
		{
			send(it->first, msg.c_str(), msg.size(), 0);
			std::cout << "Invite sent to client " << it->first << std::endl;
			return;
		}
	}
}

void Server::handleQuit(const std::string& line, int fd)
{
	Client& client = _clients[fd];
	std::string msg = ":" + client.getNickname() + "!" + client.getUser() + "@" + client.getHostname() + " QUIT :" + line.substr(5) + "\r\n";
	for (std::map<std::string, t_channel>::iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		if (it->second.members.find(fd) != it->second.members.end())
		{
			sendMessageToChannel(it->first, msg);
			leaveChannel(it->first, fd);
		}
	}
	deleteClient(fd);
}



