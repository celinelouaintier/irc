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
#include <algorithm>
#include <map>
#include <set>
#include <csignal>

#include "Client.hpp"

# define RED "\033[31m"
# define GREEN "\033[32m"
# define YELLOW "\033[33m"
# define BLUE "\033[34m"
# define MAGENTA "\033[35m"
# define CYAN "\033[36m"
# define GRAY "\033[90m"
# define BOLD "\033[1m"
# define UNDER "\033[4m"
# define BLINK "\033[5m"
# define ERASE = "\033[2K\r"
# define RESET "\033[0m"

class Server
{
    public:

        Server();
        virtual ~Server();

        //epoll
        void init(int port, const std::string &password);
        void run();
		void shutdown();

		//Exceptions
		class CreateSocketException : public std::exception
		{
			public:
				virtual const char* what() const throw();
		};

		class BindingSocketException : public std::exception
		{
			public:
				virtual const char* what() const throw();
		};

		class listeningSocketException : public std::exception
		{
			public:
				virtual const char* what() const throw();
		};

		class CreateEpollException : public std::exception
		{
			public:
				virtual const char* what() const throw();
		};

		class EpollWaitException : public std::exception
		{
			public:
				virtual const char* what() const throw();
		};

    private:
		typedef struct s_channel
		{
			std::string name; // Channel name #channelname
			std::string topic; // #channelname : channel topic
            bool allTopic; // If the topic is set by the channel operator or not
			std::set<int> members; // Set of client file descriptors (you can't have double with a set)
			std::set<int> operators;
			std::set<std::string> invitedUsers; // Users invited to the channel (en +i)
			bool isInviteOnly; // If the channel is invite only
            int limit; // Limit of users in the channel, -1 if no limit
            std::string password; // Channel password, empty if no password
		}			t_channel;

        int _epollFd;
        int _serverFd;
        struct sockaddr_in _serverAddr;
        std::string _password;
		std::map<int, Client> _clients;
		std::map<std::string, t_channel> _channels;

        void sendMessageToChannel(const std::string &channel, const std::string &msg, int fd = -1, bool sendToSelf = true);
		void leaveChannel(const std::string &channel, int fd);

		void handleNewConnection();
		void handleCommand(int clientFd);
		void deleteClient(int clientFd);
		void registerClientAndSendWelcome(int fd);
		void removeChannel(const std::string &channelName);


		void handlePrivateMessage(const std::string& line, int fd);
		void handleJoinChannel(const std::string& line, int fd);
		void handlePassword(const std::string& line, int fd);
		void handleNickname(std::string& line, int fd);
		void handleUser(const std::string& line, int fd);
		void handleKickClient(std::string& line, int fd);
		void handlePartChannel(const std::string& line, int fd);
		void handleQuit(const std::string& line, int fd);
		void handleTopic(std::string& line, int fd);
		void handleInvite(std::string& line, int fd);
        void handleMode(std::string& line, int fd);
};