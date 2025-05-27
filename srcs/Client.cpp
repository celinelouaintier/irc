#include "Client.hpp"

Client::Client()
{

}

Client::Client(int fd) : _fd(fd), _isRegistered(false)
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
		_host = rhs._host;
		_servername = rhs._servername;
		_password = rhs._password;
		_channels = rhs._channels;
		_isRegistered = rhs._isRegistered;
	}
    return *this;
}