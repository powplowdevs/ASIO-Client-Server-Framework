#ifndef CLIENT_H
#define CLIENT_H

#include "common_includes.h"

using boost::asio::ip::tcp;

class Client {
private:
    boost::asio::io_context& io_context_;
    boost::asio::io_context::work idleWork_;
    std::thread clientThread_;
    std::mutex mtx_;
    std::condition_variable recvLock_;
    TaskQueue queue;

    std::string debugPath = "./logs/client_log.txt";
    Logger clientLogger_;

    tcp::socket socket_;
    tcp::resolver resolver_;
    std::string receivedMessage_;
    tcp::resolver::iterator endpoint_iterator;

    std::string serverIp_;
    int serverPort_;

    bool isConnected_ = false;
    int timeout_ = 30;
    bool logDebug_ = false;

public:
    Client(boost::asio::io_context& io_context, const std::string& server_ip, unsigned short server_port) : io_context_(io_context), socket_(io_context), resolver_(io_context), endpoint_iterator(resolver_.resolve(server_ip, std::to_string(server_port))), idleWork_(io_context_), serverIp_(server_ip), serverPort_(server_port), clientLogger_(debugPath){
        // ...
    }

    void start(){
        clientLogger_.info("Starting client");
        clientThread_ = std::thread([this](){
            io_context_.run();
        });  
        clientThread_.detach();
    }

    void forceShutdown(){
        clientLogger_.warning("Force shutting down the client...");
        isConnected_ = false;
        io_context_.stop();
        clientLogger_.info("Client shutdown completed.");
    }

    void shutdown(bool runRemainingTasks=false){
        std::function<void()> shutOffTask = [this](){
            clientLogger_.info("Shutting down the client...");
            
            socket_.close();
            isConnected_ = false;

            if (clientThread_.joinable()){
                clientThread_.join();
            }

            io_context_.stop();

            clientLogger_.info("Client shutdown completed.");
        };

        if(!runRemainingTasks){
            shutOffTask();
        }
        else{
            queue.addTask([shutOffTask](){shutOffTask();});
        }
    }
     
    void connect(){
        clientLogger_.info("Connecting client to server | " + serverIp_ + ":" + std::to_string(serverPort_));
        
        boost::system::error_code ec;
        boost::asio::connect(socket_, endpoint_iterator, ec);
        if (ec){
            isConnected_ = false;
            clientLogger_.error("Failed to connect to server, Error: " + ec.message());
        } 
        else {
            isConnected_ = true;
            clientLogger_.info("Successfully connected to server | " + serverIp_ + ":" + std::to_string(serverPort_));
        }
    }

    void asyncReceiveMessage(){
        queue.isWorking_ = false;
        queue.isWorkingLock_.notify_one();

        if(!isConnected_) {
            clientLogger_.warning("No server connected, message not recived");
            return;
        }

        queue.addTask([this](){
            auto headerBuffer = std::make_shared<std::array<char, 4>>(); // 4 byte header

            boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
            timer.async_wait([this](const boost::system::error_code& ec){
                if(ec != boost::asio::error::operation_aborted){
                    if(!ec){
                        clientLogger_.error("Client timeout");
                    }
                    else{
                        clientLogger_.error("Timout timer error " + ec.message());
                    }
                }
            });

            boost::asio::async_read(socket_, boost::asio::buffer(*headerBuffer), 
            [this, headerBuffer, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred){
                if (!ec){
                    uint32_t messageSize;
                    std::memcpy(&messageSize, headerBuffer->data(), sizeof(uint32_t));
                    messageSize = ntohl(messageSize);
                    auto messageBuffer = std::make_shared<std::vector<char>>(messageSize);
                    if(logDebug_) clientLogger_.info("Received header: " + std::to_string(messageSize));
                    
                    // Read meassge
                    boost::asio::async_read(socket_, boost::asio::buffer(*messageBuffer),
                    [this, messageBuffer, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred){
                        if (!ec && logDebug_){
                            std::string receivedMessage(messageBuffer->begin(), messageBuffer->end());
                            clientLogger_.info("Received: " + receivedMessage);

                            timer.cancel();

                            asyncReceiveMessage(); // Continue reading
                        }
                        else {
                            clientLogger_.error("Error receiving message: " + ec.message());
                            timer.cancel();
                        }
                    });
                }
                else {
                    clientLogger_.error("Error receiving message header: " + ec.message());
                    timer.cancel();
                }
            }); 
        });
    }

    template <typename T, std::size_t Ndata>
    void asyncSendMessage(T (&message)[Ndata]){
        if (!isConnected_){
            clientLogger_.warning("No server connected, message not sent.");
            return;
        }

        queue.addTask([this, &message](){
            uint32_t messageSize = static_cast<uint32_t>(Ndata); // 4 byte header
            messageSize = htonl(messageSize);
            
            boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
            timer.async_wait([this](const boost::system::error_code& ec){
                if(ec != boost::asio::error::operation_aborted){
                    if(!ec){
                        clientLogger_.error("Server timeout");
                    }
                    else{
                        clientLogger_.error("Timout timer error " + ec.message());
                    }

                    queue.isWorking_ = false;
                    queue.isWorkingLock_.notify_one();
                }
            });

            // Send header
            socket_.async_send(boost::asio::buffer(&messageSize, sizeof(messageSize)), 
            [this, &timer, message](const boost::system::error_code& ec, std::size_t bytesTransferred){
                // Send message
                if (!ec){
                    if(logDebug_) clientLogger_.debug("Header sent: " + std::to_string(bytesTransferred) + " bytes");
                
                    socket_.async_send(boost::asio::buffer(message, Ndata),
                    [this, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred){
                        if (!ec && logDebug_) {
                            clientLogger_.debug("Message sent: " + std::to_string(bytesTransferred) + " bytes");
                        } 
                        else {
                            clientLogger_.error("Error sending message: " + ec.message());
                        }
                        timer.cancel();
                        queue.isWorking_ = false;
                        queue.isWorkingLock_.notify_one();
                    });
                } 
                else {
                    clientLogger_.error("Error sending header: " + ec.message());
                    timer.cancel();
                    queue.isWorking_ = false;
                    queue.isWorkingLock_.notify_one();
                }
            });
        });
    }

    void waitUntill(bool &condition, bool req=false){
        std::unique_lock<std::mutex> lock(mtx_);
        recvLock_.wait(lock, [this, &condition, &req](){ 
            return condition==req; 
        });
        lock.unlock();
    }

    // Setters 
    void setlogDebug_(bool value){
        logDebug_ = value;
    }
};

#endif