#pragma once

class Client
{
public:
    Client();
    Client(Client const &src);

    virtual ~Client();

    Client &operator=(Client const &rhs);

private:

};