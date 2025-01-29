#ifndef CLIENT_H
#define CLIENT_H

#include "common_includes.h"

using boost::asio::ip::tcp;

class Client {
private:
    boost::asio::io_context& io_context_;
    boost::asio::io_context::work idleWork_;
    std::thread clientThread_;

    tcp::socket socket_;
    tcp::resolver resolver_;
    std::string receivedMessage_;
    tcp::resolver::iterator endpoint_iterator;
public:
    Client(boost::asio::io_context& io_context, const std::string& server_ip, unsigned short server_port)
        : io_context_(io_context), socket_(io_context), resolver_(io_context), endpoint_iterator(resolver_.resolve(server_ip, std::to_string(server_port))), idleWork_(io_context_) {
    }

    void start() {
        clientThread_ = std::thread([this]() {
            io_context_.run();
        });  
        clientThread_.detach();
    }

    void shutdown() {
        std::cout << "Shutting down the client..." << std::endl;
        io_context_.stop();
    }
    
    void connect() {
        boost::asio::connect(socket_, endpoint_iterator);
    }

    template<typename T>   
    void receiveMessage(T userEOF) {
        boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(receivedMessage_), *userEOF,
            [this, userEOF](const boost::system::error_code& ec, std::size_t length) {
                if (!ec) {
                    std::cout << "Received: " << receivedMessage_ << std::endl;
                    receivedMessage_.clear();
                    receiveMessage(userEOF); // Recursively continue reading
                } else {
                    std::cerr << "Error receiving message: " << ec.message() << std::endl;
                }
            });
    }
};

#endif