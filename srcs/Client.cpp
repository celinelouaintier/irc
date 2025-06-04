#include "Client.hpp"

Client::Client()
{

}

Client::Client(int fd) : _fd(fd), _isRegistered(false), _nickname(""), _username(""), _hostname("localhost")
{

}

Client::Client(Client const &src)
{
    *this = src;
}

Client::~Client()
{

}

Client &Client::operator=(Client const &rhs)
{
    if (this != &rhs)
	{
		_fd = rhs._fd;
		_nickname = rhs._nickname;
		_username = rhs._username;
		_realname = rhs._realname;
		_hostname = rhs._hostname;
		_channels = rhs._channels;
		_isRegistered = rhs._isRegistered;
	}
    return *this;
}