#include "Client.hpp"

Client::Client()
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

    return *this;
}