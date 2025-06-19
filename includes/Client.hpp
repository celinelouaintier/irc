#pragma once

#include <string>
#include <vector>
#include <iostream>

class Client
{
    public:

        Client();
		Client(int fd);
        Client(Client const &src);

        virtual ~Client();

        Client &operator=(Client const &rhs);

		// Getters
		bool getIsRegistered() const { return _isRegistered; }
		bool getCorrectPassword() const { return _correctPassword; }
        std::string getNickname() const { return _nickname; }
        std::string getUsername() const { return _username; }
        std::string getUser() const { return _user; }
		std::string getHostname() const { return _hostname; }
        std::vector<std::string> getChannels() const { return _channels; }

		// Setters
		void setIsRegistered(bool isRegistered) { _isRegistered = isRegistered; }
		void setCorrectPassword(bool correctPassword) { _correctPassword = correctPassword; }
        void setNickname(const std::string &nickname) { _nickname = nickname; }
        void setUsername(const std::string &username) { _username = username; }
        void setUser(const std::string &user) { _user = user; }
		void setHostname(const std::string &hostname) { _hostname = hostname; }


    private:

        int _fd;
        bool _isRegistered;
		bool _correctPassword;
        std::string _nickname;
        std::string _username;
        std::string _user;
        std::string _realname;
        std::string _hostname;
        std::string _servername;
        std::vector<std::string> _channels;

};