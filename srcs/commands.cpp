#include "Server.hpp"

int stoii(std::string str)
{
	std::stringstream s(str);
	int i;

	if (!(s >> i) || !s.eof())
		return -1;
	return i;
}

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
			msg = ":" + _clients[fd].getHostname() + " 401 " + _clients[fd].getNickname() + " " + dest + " :No such nick\r\n";
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
    std::string pass;
	std::string msg;
	if (channel.empty())
	{
		msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " PART :Not enough parameters\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

    if (channel.find(' ') != std::string::npos)
    {
        pass = channel.substr(channel.find(' ') + 1);
        channel = channel.substr(0, channel.find(' '));
    }

	if (_channels.find(channel) == _channels.end()) {
		_channels[channel] = t_channel();
		_channels[channel].name = channel;
		_channels[channel].members.insert(fd);
		_channels[channel].operators.insert(fd);
		_channels[channel].isInviteOnly = false;
		_channels[channel].topic = "";
        _channels[channel].limit = -1;
        _channels[channel].password = "";
        _channels[channel].allTopic = false;
		std::cout << "Channel " << channel << " created" << std::endl;
	}
	else if (_channels[channel].isInviteOnly && _channels[channel].invitedUsers.find(_clients[fd].getNickname()) == _channels[channel].invitedUsers.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 473 " + _clients[fd].getNickname() + " " + channel + " :Cannot join channel (+i)\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
	else if (_channels[channel].members.find(fd) != _channels[channel].members.end())
		return (void)send(fd, "You are already a member of this channel\n", 42, 0);
    else if (_channels[channel].limit != -1 && (int)_channels[channel].members.size() >= _channels[channel].limit)
    {
		msg = ":" + _clients[fd].getHostname() + " 471 " + _clients[fd].getNickname() + " " + channel + " :Cannot join channel (+l)\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
    if (!_channels[channel].password.empty() && pass != _channels[channel].password)
	{
		msg = ":" + _clients[fd].getHostname() + " 475 " + _clients[fd].getNickname() + " " + channel + " :Cannot join channel (+k)\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}
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
	if (!_channels[channel].allTopic && _channels[channel].operators.find(fd) == _channels[channel].operators.end())
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
	std::string msg;
	std::string info = line.substr(7);
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

void Server::handleMode(std::string &line, int fd)
{
	line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());


    std::string msg;
    std::string info = line.substr(5);
    if (info.empty())
    {
        msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " :Not enough parameters\r\n";
        return (void)send(fd, msg.c_str(), msg.size(), 0);
    }

    std::string channel = info.substr(0, info.find(' '));
    if (channel.empty())
    {
        msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " :No target specified\r\n";
        return(void)send(fd, msg.c_str(), msg.size(), 0);
    }

	if (_channels.find(channel) == _channels.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 403 " + _clients[fd].getNickname() + " " + channel + " :No such channel\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

	if (_channels[channel].operators.find(fd) == _channels[channel].operators.end())
	{
		msg = ":" + _clients[fd].getHostname() + " 482 " + _clients[fd].getNickname() + " " + channel + " :You're not channel operator\r\n";
		return (void)send(fd, msg.c_str(), msg.size(), 0);
	}

    std::string mess = info.substr(info.find(' ') + 1);
    if (mess.empty())
    {
        msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " :No mode specified\r\n";
        return (void)send(fd, msg.c_str(), msg.size(), 0);
    }

    std::string mode = mess;
    std::string arg = "";
    if (mess.find(' ') != std::string::npos)
    {
        mode = mess.substr(0, mess.find(' '));
        arg = mess.substr(mess.find(' ') + 1);
    }

    if (mode.empty())
    {
        msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " :No mode specified\r\n";
        return (void)send(fd, msg.c_str(), msg.size(), 0);
    }

    bool add = mode[0] == '+';
    if (mode [0] != '+' && mode[0] != '-')
    {
        msg = ":" + _clients[fd].getHostname() + " 501 " + _clients[fd].getNickname() + " :Unknown mode\r\n";
        return (void)send(fd, msg.c_str(), msg.size(), 0);
    }
    for (size_t i = 1; i < mode.size(); ++i)
    {
        if (mode[i] == 'i')
        {
            if (add)
            {
                if (_channels[channel].isInviteOnly)
                {
                    msg = ":" + _clients[fd].getHostname() + " 221 " + _clients[fd].getNickname() + " " + channel + " :Channel is already invite-only\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].isInviteOnly = true;
            }
            else
            {
                if (!_channels[channel].isInviteOnly)
                {
                    msg = ":" + _clients[fd].getHostname() + " 221 " + _clients[fd].getNickname() + " " + channel + " :Channel is not invite-only\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].isInviteOnly = false;
            }
        }
        else if (mode[i] == 'o')
        {
            if (arg.empty())
            {
                msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " :No nickname specified\r\n";
                return (void)send(fd, msg.c_str(), msg.size(), 0);
            }
            int targetFd = -1;
            for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
                if (it->second.getNickname() == arg && it->second.getIsRegistered())
                    targetFd = it->first;
			if (targetFd == -1)
			{
				msg = ":" + _clients[fd].getHostname() + " 401 " + _clients[fd].getNickname() + " " + arg + " :No such nick\r\n";
				return (void)send(fd, msg.c_str(), msg.size(), 0);
			}
            if (add)
            {
                if (_channels[channel].operators.find(targetFd) != _channels[channel].operators.end())
                {
                    msg = ":" + _clients[fd].getHostname() + " 221 " + _clients[fd].getNickname() + " " + channel + " :" + _clients[targetFd].getNickname() + " is already channel operator\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].operators.insert(targetFd);
            }
            else
            {
                if (_channels[channel].operators.find(targetFd) == _channels[channel].operators.end())
                {
                    msg = ":" + _clients[fd].getHostname() + " 221 " + _clients[fd].getNickname() + " " + channel + " :" + _clients[targetFd].getNickname() + " is not a channel operator\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].operators.erase(targetFd);
            }
        }
        else if (mode[i] == 'l')
        {
            if (add)
            {
                if (arg.empty())
                {
                    msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " MODE :No limit specified\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                int limit = stoii(arg);
                if (limit < 0)
                {
                    msg = ":" + _clients[fd].getHostname() + " 501 " + _clients[fd].getNickname() + " MODE :Invalid limit\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].limit = limit;
            }
            else
                _channels[channel].limit = -1;
        }
        else if (mode[i] == 'k')
        {
            if (add)
            {
                if (arg.empty())
                {
                    msg = ":" + _clients[fd].getHostname() + " 461 " + _clients[fd].getNickname() + " MODE :No password specified\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].password = arg;
            }
            else
                _channels[channel].password = "";
        }
        else if (mode[i] == 't')
        {
            if (add)
            {
                if (_channels[channel].allTopic)
                {
                    msg = ":" + _clients[fd].getHostname() + " 221 " + _clients[fd].getNickname() + " " + channel + " :Channel topic is already set by the channel operator\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].allTopic = true;
            }
            else
            {
                if (!_channels[channel].allTopic)
                {
                    msg = ":" + _clients[fd].getHostname() + " 221 " + _clients[fd].getNickname() + " " + channel + " :Channel topic is not set by the channel operator\r\n";
                    return (void)send(fd, msg.c_str(), msg.size(), 0);
                }
                _channels[channel].allTopic = false;
            }
        }
        else
        {
            msg = ":" + _clients[fd].getHostname() + " 472 " + _clients[fd].getNickname() + " MODE :" + mode[i] + " :Unknown mode\r\n";
            return (void)send(fd, msg.c_str(), msg.size(), 0);
        }
    }


}

void Server::handleQuit(const std::string& line, int fd)
{
	if (line.size() > 4 && line[4] != ' ')
		return;

	Client& client = _clients[fd];
	std::string message = "";
	if (line.size() > 5)
		message = line.substr(5);
	std::string msg = ":" + client.getNickname() + "!" + client.getUser() + "@" + client.getHostname() + " QUIT " + message + "\r\n";
	for (std::map<std::string, t_channel>::iterator it = _channels.begin(); it != _channels.end(); )
	{
		if (it->second.members.find(fd) != it->second.members.end())
		{
			std::string channelName = it->first;
			++it;
			sendMessageToChannel(channelName, msg);
			leaveChannel(channelName, fd);
		}
		else
			++it;
	}
	std::cout << "Client " << fd << " has quit the server" << std::endl;
	deleteClient(fd);
}



