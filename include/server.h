#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <iostream>

// TODO: ADD COMMENTS TO THIS FILE

using boost::asio::ip::tcp;

class Server {
    public:
        Server(boost::asio::io_context& io_context, unsigned short port);
        void start();

    private:
        void asyncAcceptConnection();

        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;
        std::vector<std::shared_ptr<tcp::socket>> clients_;

};

#endif