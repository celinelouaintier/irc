#pragma once

#include <csignal>
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
	private:
		typedef struct s_channel
		{
			std::string name; 
			std::string topic; 
			std::set<int> members; 
			std::set<int> operators;
			std::set<int> invitedUsers;
			bool isInviteOnly; 
		}			t_channel;

		int _epollFd;
		int _serverFd;
		struct sockaddr_in _serverAddr;
		std::string _password;
		std::map<int, *Client> _clientMap;
		std::map<std::string, t_channel> _channels;

	public:

		Server();
		Server(Server const &src);
		Server &operator=(Server const &rhs);
		virtual ~Server();

		void init(int port, const std::string &password);
		void run();
		void shutdown();

		void defineNickname(int clientFd, const std::string &nickname);
		void defineUsername(int clientFd, const std::string &username);
		void joinChannel(int clientFd, const std::string &channel);
		void sendMessage(int clientFd, const std::string &message);

		void kickClient(int clientFd);
		void inviteClient(int clientFd, const std::string &channel);
		void topicChannel(int clientFd, const std::string &channel, const std::string &topic);
		void setMode(int clientFd, const std::string &mode);

		void deleteClient(int clientFd);
		void handleNewConnection();
		void handleClientMessage(int clientFd);


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

};
