#ifndef SERVER_H
#define SERVER_H

#include "common_includes.h"

using boost::asio::ip::tcp;

class Server {
private:
    boost::asio::io_context& io_context_;
    boost::asio::io_context::work idleWork_;
    std::thread serverThread_;

    tcp::acceptor acceptor_;
    std::vector<std::shared_ptr<tcp::socket>> clients_;

    std::mutex mtx_;
    std::condition_variable clientsLock;
    int connectedClientsCount = 0;
public:
    Server(boost::asio::io_context& io_context, unsigned short port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), idleWork_(io_context_) {
        std::cout << "Server started on port " << port << std::endl;
    }

    void start() {
        serverThread_ = std::thread([this]() {
            io_context_.run();
        });
        serverThread_.detach();
    }

    void shutdown() {
        std::cout << "Shutting down the server..." << std::endl;
        io_context_.stop();
        acceptor_.close();
    }

    void asyncAcceptConnection() {
        // Make socket for new client
        auto socket = std::make_shared<tcp::socket>(io_context_);

        // Accept new client and handle the connection
        acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
            if (!ec) {
                clients_.push_back(socket);
                std::cout << "Client connected: " << socket->remote_endpoint() << std::endl;

                // Nofity clientsLock
                connectedClientsCount++;
                clientsLock.notify_one();

                asyncAcceptConnection(); // Keep waiting for connections
            } else {
                std::cerr << "Error while accepting connection: " << ec.message() << std::endl;
            }
        });
    }

    template <typename T, typename T2, std::size_t Ndata, std::size_t Neof>
    void asyncSendMessageToAll(T (&message)[Ndata], T2 (&userEOF)[Neof]) {
        if (clients_.empty()) {
            std::cout << "No clients connected, message not sent." << std::endl;
            return;
        }
        std::cout << "attempt to create a new msg\n msg: " << message << " msgN: " << Ndata << "\nEOF: " << userEOF << " Neof: " << Neof <<  std::endl;
        for (auto client : clients_) {
            boost::asio::async_write(*client, boost::asio::buffer(message, Ndata),
                [message](const boost::system::error_code& ec, std::size_t bytesTransferred) {
                    if (!ec) {
                        std::cout << "Packet sent: " << bytesTransferred << " bytes\n";
                    } else {
                        std::cout << "Error sending packet: " << ec.message() << std::endl;
                    }
                });

            // Send EOF
            boost::asio::async_write(*client, boost::asio::buffer(userEOF, Neof),
                [userEOF](const boost::system::error_code& ec, std::size_t bytesTransferred) {
                    if (!ec) {
                        std::cout << "\nEOF sent: '" << userEOF << "'\n";
                    } else {
                        std::cout << "Error sending packet EOF: " << ec.message() << std::endl;
                    }
                });
        }
    }

    template <typename T, typename T2, std::size_t Ndata, std::size_t Neof>
    void asyncSendMessage(std::shared_ptr<tcp::socket> client, T (&message)[Ndata], T2 (&userEOF)[Neof]) {
        if (clients_.empty()) {
            std::cout << "No clients connected, message not sent." << std::endl;
            return;
        }

        std::cout << "attempt to create a new msg\n msg: " << message << " msgN: " << Ndata << "\nEOF: " << userEOF << " Neof: " << Neof <<  std::endl;
        boost::asio::async_write(*client, boost::asio::buffer(message, Ndata),
            [message](const boost::system::error_code& ec, std::size_t bytesTransferred) {
                if (!ec) {
                    std::cout << "Packet sent: " << bytesTransferred << " bytes\n";
                } else {
                    std::cout << "Error sending packet: " << ec.message() << std::endl;
                }
            });

        // Send EOF
        boost::asio::async_write(*client, boost::asio::buffer(userEOF, Neof),
            [userEOF](const boost::system::error_code& ec, std::size_t bytesTransferred) {
                if (!ec) {
                    std::cout << "\nEOF sent: '" << userEOF << "'\n";
                } else {
                    std::cout << "Error sending packet EOF: " << ec.message() << std::endl;
                }
            });
    }

    void waitForConnections(int x = 1) {
        std::unique_lock<std::mutex> lock(mtx_);
        // Wait until enough clients are connected
        clientsLock.wait(lock, [this, x]() { 
            std::cout << "Waiting for " << x << " connections. Current: " << connectedClientsCount << std::endl;
            return connectedClientsCount >= x; 
        });
    }

    std::vector<std::shared_ptr<tcp::socket>> getClients(){
        return clients_;
    }

    std::shared_ptr<tcp::socket> getClient(int index){
        return clients_[index];
    }
};

#endif


