#include "client.h"

Client::Client(boost::asio::io_context& io_context, const std::string& server_ip, unsigned short server_port)
    : io_context_(io_context), socket_(io_context), resolver_(io_context) {

    tcp::resolver::query query(server_ip, std::to_string(server_port));
    tcp::resolver::iterator endpoint_iterator = resolver_.resolve(query);       
    
    boost::asio::connect(socket_, endpoint_iterator);
}

template<typename T>
void Client::start(T userEOF){
    receiveMessage(userEOF);
}

template<typename T>
void Client::receiveMessage(T userEOF){
     boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(receivedMessage_), userEOF,
    [this, userEOF](const boost::system::error_code& ec, std::size_t length) {
        if (!ec) {
            std::cout << "Received: " << receivedMessage_ << std::endl;
            receivedMessage_.clear();
            receiveMessage(userEOF);
        } else {
            std::cerr << "Error receiving message: " << ec.message() << std::endl;
        }
    });
}