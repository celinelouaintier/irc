#include "Server.hpp"

// KICK command to kick a client from a channel
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

void Server::handlePartChannel(const std::string& line, int fd)
{
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
	std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " PART " + channel;
	if (!message.empty())
		msg += " :" + message;
	msg += "\r\n";
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

void Server::handleQuit(const std::string& line, int fd)
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



