#pragma once

class Server
{
public:
    Server();
    Server(Server const &src);

    virtual ~Server();

    Server &operator=(Server const &rhs);

private:

};