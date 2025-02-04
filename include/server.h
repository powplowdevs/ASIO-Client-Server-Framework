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
    std::condition_variable clientsLock_;
    std::condition_variable streamLock_;

    std::string debugPath = "./logs/server_log.txt";
    Logger serverLogger_;    

    int connectedClientsCount_ = 0;
    int timeout_ = 30;
    bool isWorking_ = false;
    bool acceptingConnections_ = true;
    bool logDebug_ = false;

public:
    Server(boost::asio::io_context& io_context, unsigned short port) : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), idleWork_(io_context_), serverLogger_(debugPath){
        // ...
    }

    void start(){
        serverLogger_.info("Starting server | " + acceptor_.local_endpoint().address().to_string() + ":" + std::to_string(acceptor_.local_endpoint().port()));
        serverThread_ = std::thread([this](){
            io_context_.run();
        });
        serverThread_.detach();
    }

    void forceShutdown(){
        serverLogger_.warning("Force shutting down the server!");
        io_context_.stop();
        acceptor_.close();
        serverLogger_.info("Server shutdown completed.");
    }

    void shutdown(){
        serverLogger_.info("Shutting down the server...");
        setacceptingConnections_(false);
        for (auto client : clients_){
            client->shutdown(tcp::socket::shutdown_both);
            client->close();
        }

        if (serverThread_.joinable()){
            serverThread_.join();
        }

        io_context_.stop();
        acceptor_.close();

        serverLogger_.info("Server shutdown completed.");
    }

    void asyncAcceptConnection(){
        // If accepting connections
        if(!acceptingConnections_){
            serverLogger_.info("Server stopped accepting connections");
            return;
        }
        // Make socket for new client
        auto socket = std::make_shared<tcp::socket>(io_context_);

        // Accept new client and handle the connection
        acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec){
            if (!ec){
                clients_.push_back(socket);
                serverLogger_.info("Client connected | " + socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port()));

                // Notify clientsLock_
                connectedClientsCount_++;
                clientsLock_.notify_one();

                asyncAcceptConnection(); // Keep waiting for connections
            } 
            else {
                serverLogger_.error("Error while accepting connection: " + ec.message());
            }
        });
    }

    template <typename T, std::size_t Ndata>
    void asyncSendMessageToAll(T (&message)[Ndata]){
        if (clients_.empty()){
            serverLogger_.warning("No clients connected, message not sent.");
            return;
        }   

        waitUntill(isWorking_);
        isWorking_ = true;
        
        std::atomic<int> runningJobs(clients_.size());

        for (auto& client : clients_){
            uint32_t messageSize = static_cast<uint32_t>(Ndata);
            messageSize = htonl(messageSize);

            boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
            timer.async_wait([this](const boost::system::error_code& ec){
                if(ec != boost::asio::error::operation_aborted){
                    if(!ec){
                        serverLogger_.error("Server timeout");
                    }
                    else{
                        serverLogger_.error("Timout timer error " + ec.message());
                    }
                }
            });

            boost::asio::async_write(*client, boost::asio::buffer(&messageSize, sizeof(messageSize)),
            [this, client, message, &runningJobs, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred) mutable {
                if (!ec){
                    if(logDebug_) serverLogger_.debug("Header sent: " + std::to_string(bytesTransferred) + " bytes");

                    boost::asio::async_write(*client, boost::asio::buffer(message, Ndata),
                        [this, &runningJobs, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred) mutable {
                            if (!ec && logDebug_){
                                serverLogger_.debug("Message sent: " + std::to_string(bytesTransferred) + " bytes");
                            } 
                            else {
                                serverLogger_.error("Error sending message: " + ec.message());
                            }
                            runningJobs.fetch_sub(1);
                            if(runningJobs.load() <= 0){
                                isWorking_ = false;
                                timer.cancel();
                                streamLock_.notify_one();
                            }
                        });
                } 
                else {
                    serverLogger_.error("Error sending header: " + ec.message());
                    runningJobs.fetch_sub(1); 
                    if (runningJobs.load() <= 0){
                        isWorking_ = false;
                        timer.cancel();
                        streamLock_.notify_one();
                    }
                }
            });            
        }
    }

    template <typename T, std::size_t Ndata>
    void asyncSendMessage(std::shared_ptr<tcp::socket>& client, T (&message)[Ndata]){
        if (clients_.empty()){
            serverLogger_.warning("No clients connected, message not sent.");
            return;
        }

        waitUntill(isWorking_);
        isWorking_ = true;
        std::atomic<int> runningJobs(clients_.size());

        uint32_t messageSize = static_cast<uint32_t>(Ndata); // 4 byte header
        messageSize = htonl(messageSize);
        
        boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
        timer.async_wait([this](const boost::system::error_code& ec){
            if(ec != boost::asio::error::operation_aborted){
                if(!ec){
                    serverLogger_.error("Server timeout");
                }
                else{
                    serverLogger_.error("Timout timer error " + ec.message());
                }
            }
        });

        boost::asio::async_write(*client, boost::asio::buffer(&messageSize, sizeof(messageSize)),
        [this, client, message, &runningJobs, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred) mutable {
            if (!ec){
                if(logDebug_) serverLogger_.debug("Header sent: " + std::to_string(bytesTransferred) + " bytes");

                boost::asio::async_write(*client, boost::asio::buffer(message, Ndata),
                    [this, &runningJobs, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred) mutable {
                        if (!ec && logDebug_){
                            serverLogger_.debug("Message sent: " + std::to_string(bytesTransferred) + " bytes");
                        } 
                        else{
                            serverLogger_.error("Error sending message: " + ec.message());
                        }
                        runningJobs.fetch_sub(1); 
                        if (runningJobs.load() <= 0){
                            isWorking_ = false;
                            timer.cancel();
                            streamLock_.notify_one();
                        }
                    });
            } 
            else {
                serverLogger_.error("Error sending header: " + ec.message());
                runningJobs.fetch_sub(1); 
                if (runningJobs.load() <= 0){
                    isWorking_ = false;
                    timer.cancel();
                    streamLock_.notify_one();
                }
            }
        });
    }

    void asyncReceiveMessage(){
        auto headerBuffer = std::make_shared<std::array<char, 4>>(); // 4 byte header
        
        //waitUntill(isWorking_);
        isWorking_ = true;

        boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
        timer.async_wait([this](const boost::system::error_code& ec){
            if(ec != boost::asio::error::operation_aborted){
                if(!ec){
                    serverLogger_.error("Server timeout");
                }
                else{
                    serverLogger_.error("Timout timer error " + ec.message());
                }
            }
        });

        for(auto& client : clients_){
            boost::asio::async_read(*client, boost::asio::buffer(*headerBuffer), 
            [this, headerBuffer, &timer, client](const boost::system::error_code& ec, std::size_t bytesTransferred){
                if (!ec){
                    uint32_t messageSize;
                    std::memcpy(&messageSize, headerBuffer->data(), sizeof(uint32_t));
                    messageSize = ntohl(messageSize);
                    auto messageBuffer = std::make_shared<std::vector<char>>(messageSize);
                    if(logDebug_) serverLogger_.info("Received header (" + std::to_string(messageSize) + " bytes) from client " + client->remote_endpoint().address().to_string() + ":" + std::to_string(client->remote_endpoint().port()));
                    
                    // Read meassge
                    boost::asio::async_read(*client, boost::asio::buffer(*messageBuffer),
                    [this, messageBuffer, &timer, client](const boost::system::error_code& ec, std::size_t bytesTransferred){
                        if (!ec && logDebug_){
                            std::string receivedMessage(messageBuffer->begin(), messageBuffer->end());
                            serverLogger_.info("Received from " + client->remote_endpoint().address().to_string() + ":" + std::to_string(client->remote_endpoint().port()) + " -> " + receivedMessage);

                            isWorking_ = false;
                            streamLock_.notify_one();
                            timer.cancel();

                            asyncReceiveMessage(); // Continue reading
                        }
                        else {
                            serverLogger_.error("Error receiving message: " + ec.message());
                            isWorking_ = false;
                            streamLock_.notify_one();
                        }
                    });
                }
                else {
                    serverLogger_.error("Error receiving message header: " + ec.message());
                    isWorking_ = false;
                    streamLock_.notify_one();
                }
            });
        } 
    }

    void waitForConnections(int x = 1){
        std::unique_lock<std::mutex> lock(mtx_);
        // Wait until enough clients are connected
        clientsLock_.wait(lock, [this, x](){ 
            serverLogger_.info("Waiting for " + std::to_string(x) + " connections. Current: " + std::to_string(connectedClientsCount_));
            return connectedClientsCount_ >= x; 
        });
        lock.unlock();
    }

    void waitUntill(bool &condition, bool req=false){
        std::unique_lock<std::mutex> lock(mtx_);
        streamLock_.wait(lock, [this, &condition, &req](){ 
            return condition==req; 
        });
        lock.unlock();
    }

    // Getters
    std::vector<std::shared_ptr<tcp::socket>> getClients(){
        return clients_;
    }

    std::shared_ptr<tcp::socket> getClient(int index){
        return clients_[index];
    }

    int getClientsAmount(){
        return clients_.size();
    }

    // Setters
    void setacceptingConnections_(bool value){
        acceptingConnections_ = value;
    }

    void setlogDebug_(bool value){
        logDebug_ = value;
    }

    void setTimeout(int value){
        timeout_ = value;
    }

};

#endif


