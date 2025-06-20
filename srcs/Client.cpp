#include "Client.hpp"

Client::Client()
{
 
}

Client::Client(int fd) : _fd(fd), _isRegistered(false), _correctPassword(false), _nickname(""), _username(""), _hostname("localhost")
{

}

Client::~Client()
{

}
