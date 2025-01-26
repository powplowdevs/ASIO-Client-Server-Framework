#ifndef CLIENT_H
#define CLIENT_H

#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

class Client {
    public:
        Client(boost::asio::io_context& io_context, const std::string& serverIp, unsigned short serverPort);
        void start();

    private:
        void receiveMessage();

        boost::asio::io_context& io_context_;
        tcp::socket socket_;
        tcp::resolver resolver_;
        std::string receivedMessage_;
};

#endif
