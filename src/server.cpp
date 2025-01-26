#include "server.h"

using std::cout;
using std::cin;
using std::cerr;
using std::endl;

// Create the server constructor
Server::Server(boost::asio::io_context& io_context, unsigned short port)
    : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "Server started on port " << port << std::endl;
}

void Server::start(){
    asyncAcceptConnection();
}

void Server::asyncAcceptConnection(){
    // Make socket for new client
    auto socket = std::make_shared<tcp::socket>(io_context_);
    
    // Accept new client    Socket ~ capture lsit ~ params for lamda func
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if(!ec){
            clients_.push_back(socket);
            cout << "Client connected: " << socket->remote_endpoint() << endl;

            asyncAcceptConnection(); // Keep waiting for connections
        }
        else{
            cerr << "Error while accepting connection: " << ec.message() << endl;
        }
    });
}