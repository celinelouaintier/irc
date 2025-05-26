#pragma once


#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <vector>

class Server
{
    public:

        Server();
        virtual ~Server();

        //epoll
        void init(int port, const std::string &password);
        void run(int port, const std::string &password);
        void shutdown();

        void defineNickname(int clientFd, const std::string &nickname);
        void defineUsername(int clientFd, const std::string &username);
        void joinChannel(int clientFd, const std::string &channel);
        void sendMessage(int clientFd, const std::string &message);


        void kickClient(int clientFd);
        void inviteClient(int clientFd, const std::string &channel);
        void topicChannel(int clientFd, const std::string &channel, const std::string &topic);
        void setMode(int clientFd, const std::string &mode);

    private:

        bool _isAdmin;
        // tout ce qu'il y a dans le main pour le moment
        int _epollFd;
        std::vector<int> _clientFds;
        std::string _password;
        struct sockaddr_in _serverAddr;
        int _serverFd;
};