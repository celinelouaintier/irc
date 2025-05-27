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

		// Setters
		void setIsRegistered(bool isRegistered) { _isRegistered = isRegistered; }

    private:

        int _fd;
        std::string _nickname;
        std::string _username;
        std::string _realname;
        std::string _host;
        std::string _servername;
        std::string _password;
        std::vector<std::string> _channels;

        bool _isRegistered;
};