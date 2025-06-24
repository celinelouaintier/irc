#include "Server.hpp"

// KICK command to kick a client from a channel
void Server::handleKickClient(std::string& line, int fd)
{
	std::string message = "";
	std::string msg;
	std::string info = line.substr(5);
	if (info.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " KICK :Not enough parameters\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string channel = info.substr(0, info.find(' '));
	if (_channels.find(channel) == _channels.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 403 " + _clients[fd].getNickname() + " " + channel + " :No such channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string nickname = info.substr(info.find(' ') + 1, info.find(':') - info.find(' ') - 2);
	if (nickname.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " KICK :No nickname given\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	if (_clients[fd].getNickname() == nickname)
		return (void)send(fd, "You cannot kick yourself\n", 25, 0);

	if (info.find(':') != std::string::npos)
		message = info.substr(info.find(':') + 1);

	if (_channels[channel].members.find(fd) == _channels[channel].members.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 442 " + _clients[fd].getNickname() + " " + channel + " :You're not on that channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 482 " + _clients[fd].getNickname() + " " + channel + " :You're not channel operator\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	int kickfd = -1;
	for (std::set<int>::iterator it = _channels[channel].members.begin(); it != _channels[channel].members.end(); ++it)
	{
		if (_clients[*it].getNickname() == nickname)
			kickfd = *it;
	}
	if (kickfd == -1)
	{
		msg = ":" + _clients[fd].getHostname() + " 441 " + _clients[fd].getNickname() + " " + nickname + " " + channel + " :They aren't on that channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " KICK " + channel + " " + nickname + " :" + message + "\r\n";
	sendMessageToChannel(channel, msg);

	if (kickfd > 0)
	{
		msg = "You have been kicked from " + channel + " by " + _clients[fd].getNickname();
		if (!message.empty())
			msg += " : " + message;
		msg += "\r\n";
		send(kickfd, msg.c_str(), msg.size(), 0);
		leaveChannel(channel, kickfd);
	}
}

// PRIVMSG command
void Server::handlePrivateMessage(const std::string& line, int fd)
{
	std::string info = line.substr(8);
	std::string msg;
	if (info.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " PRIVMSG :Not enough parameters\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string dest = info.substr(0, info.find(' '));
	if (dest.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " PRIVMSG :No destination specified\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	if (dest[0] == '#')
	{
		if (_channels.find(dest) == _channels.end())
		{
			msg = ":" + _clients[fd].getHostname() + " 403 " + _clients[fd].getNickname() + " " + dest + " :No such channel\r\n";
			return (void)send(fd, msg.c_str(), msg.size(), 0);
		}
		if (_channels[dest].members.find(fd) == _channels[dest].members.end())
		{
			msg = ":" + _clients[fd].getHostname() + " 442 " + _clients[fd].getNickname() + " " + dest + " :You're not on that channel\r\n";
			return (void)send(fd, msg.c_str(), msg.size(), 0);
		}
	}
	else
	{
		bool found = false;
		for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second.getNickname() == dest && it->second.getIsRegistered())
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			msg = ":" + _clients[fd].getHostname() + " 401 " + _clients[fd].getNickname() + " " + dest + " :No such nick/channel\r\n";
			return (void)send(fd, msg.c_str(), msg.size(), 0);
		}
	}

	std::string message = info.substr(info.find(':') + 1);
	if (message.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 412 " + _clients[fd].getNickname() + " :No text to send\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " PRIVMSG " + dest + " :" + message + "\r\n";
	if (dest[0] == '#')
		sendMessageToChannel(dest, msg, fd, false);
	else 
	{
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
}

// JOIN command to join a channel
void Server::handleJoinChannel(const std::string& line, int fd)
{
	std::string channel = line.substr(5);

	if (channel.empty())
		return (void)send(fd, "You must specify a channel to join\n", 36, 0);
	if (_channels.find(channel) == _channels.end()) {
		_channels[channel] = t_channel();
		_channels[channel].name = channel;
		_channels[channel].members.insert(fd);
		_channels[channel].operators.insert(fd);
		_channels[channel].isInviteOnly = false;
		_channels[channel].topic = ""; 
		std::cout << "Channel " << channel << " created" << std::endl;
	}
	else if (_channels[channel].isInviteOnly && _channels[channel].invitedUsers.find(_clients[fd].getNickname()) == _channels[channel].invitedUsers.end())
		return (void)send(fd, "This channel is invite-only\n", 28, 0);
	else if (_channels[channel].members.find(fd) != _channels[channel].members.end())
		return (void)send(fd, "You are already a member of this channel\n", 42, 0);
	else {
		_channels[channel].members.insert(fd);
		std::cout << "Client " << fd << " joined channel " << channel << std::endl;
		std::string msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " JOIN " + channel + "\r\n";
		sendMessageToChannel(channel, msg);

		std::string topicMsg = ":" + _clients[fd].getHostname() + " 331 " + _clients[fd].getNickname() + " " + channel + " :No topic is set\r\n";
		if (!_channels[channel].topic.empty())
			topicMsg = ":" + _clients[fd].getHostname() + " 332 " + _clients[fd].getNickname() + " " + channel + " :" + _channels[channel].topic + "\r\n";
		send(fd, topicMsg.c_str(), topicMsg.size(), 0);
	}
}

void Server::handlePartChannel(const std::string& line, int fd)
{
	std::string message = "";
	std::string msg;
	std::string info = line.substr(5);
	if (info.empty()) 
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " PART :Not enough parameters\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string channel = info;
	if (info.find(':') != std::string::npos)
	{
		message = info.substr(info.find(':') + 1);
		channel = info.substr(0, info.find(' '));
	}
	if (_channels.find(channel) == _channels.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 403 " + _clients[fd].getNickname() + " " + channel + " :No such channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	if (_channels[channel].members.find(fd) == _channels[channel].members.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 442 " + _clients[fd].getNickname() + " " + channel + " :You're not on that channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " PART " + channel + " :" + message + "\r\n";
	sendMessageToChannel(channel, msg);

	leaveChannel(channel, fd);
}


void Server::handleTopic(std::string &line, int fd)
{
	line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

	std::string info = line.substr(6);
	std::string msg;
	if (info.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " TOPIC :Not enough parameters\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string channel = info.substr(0, info.find(' '));
	if (_channels.find(channel) == _channels.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 403 " + _clients[fd].getNickname() + " " + channel + " :No such channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	if (_channels[channel].members.find(fd) == _channels[channel].members.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 442 " + _clients[fd].getNickname() + " " + channel + " :You're not on that channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 482 " + _clients[fd].getNickname() + " " + channel + " :You're not channel operator\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string topic = line.substr(line.find(':') + 1);
	_channels[channel].topic = topic;

	msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " TOPIC " + channel + " :" + topic + "\r\n";
	sendMessageToChannel(channel, msg);
}

void Server::handleInvite(std::string& line, int fd)
{
	std::string info = line.substr(7);
	std::string msg;
	if (info.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " INVITE :Not enough parameters\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string nickname = info.substr(0, info.find(' '));
	if (nickname.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " INVITE :No nickname given\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	std::string channel = info.substr(info.find(' ') + 1);
	if (_channels.find(channel) == _channels.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 403 " + _clients[fd].getNickname() + " " + channel + " :No such channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	if (_channels[channel].members.find(fd) == _channels[channel].members.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 442 " + _clients[fd].getNickname() + " " + channel + " :You're not on that channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 482 " + _clients[fd].getNickname() + " " + channel + " :You're not channel operator\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	int targetFd = -1;
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		if (it->second.getNickname() == nickname && it->second.getIsRegistered())
			targetFd = it->first;

	if (_channels[channel].members.find(targetFd) != _channels[channel].members.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 443 " + _clients[fd].getNickname() + " " + channel + " :User is already in the channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	_channels[channel].invitedUsers.insert(nickname);
	msg = ":" + _clients[fd].getNickname() + "!" + _clients[fd].getUser() + "@" + _clients[fd].getHostname() + " INVITE " + nickname + " " + channel + "\r\n";
	send(fd, msg.c_str(), msg.size(), 0);
	send(targetFd, msg.c_str(), msg.size(), 0);
	std::cout << "Invite sent to client " << targetFd << std::endl;


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



