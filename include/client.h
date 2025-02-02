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

    std::string debugPath = "./logs/client_log.txt";
    Logger clientLogger_;

    tcp::socket socket_;
    tcp::resolver resolver_;
    std::string receivedMessage_;
    tcp::resolver::iterator endpoint_iterator;

    std::string serverIp_;
    int serverPort_;

    bool isConntected = false;
    bool isWorking_ = false;

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
        isConntected = false;
        io_context_.stop();
        clientLogger_.info("Client shutdown completed.");
    }

    void shutdown(){
        clientLogger_.info("Shutting down the client...");
        
        socket_.close();
        isConntected = false;

        if (clientThread_.joinable()){
            clientThread_.join();
        }

        io_context_.stop();

        clientLogger_.info("Client shutdown completed.");
    }
     
    void connect(){
        clientLogger_.info("Connecting client to server | " + serverIp_ + ":" + std::to_string(serverPort_));
        
        boost::system::error_code ec;
        boost::asio::connect(socket_, endpoint_iterator, ec);
        if (ec){
            isConntected = false;
            clientLogger_.error("Failed to connect to server, Error: " + ec.message());
        } 
        else {
            isConntected = true;
            clientLogger_.info("Successfully connected to server | " + serverIp_ + ":" + std::to_string(serverPort_));
        }
    }

    void asyncReceiveMessage(){
        auto headerBuffer = std::make_shared<std::array<char, 4>>(); // 4 byte header
        
        waitUntill(isWorking_);
        isWorking_ = true;

        boost::asio::async_read(socket_, boost::asio::buffer(*headerBuffer), 
        [this, headerBuffer](const boost::system::error_code& ec, std::size_t bytesTransferred){
            if (!ec){
                uint32_t messageSize;
                std::memcpy(&messageSize, headerBuffer->data(), sizeof(uint32_t));
                messageSize = ntohl(messageSize);
                auto messageBuffer = std::make_shared<std::vector<char>>(messageSize);
                clientLogger_.info("Received header: " + std::to_string(messageSize));
                
                // Read meassge
                boost::asio::async_read(socket_, boost::asio::buffer(*messageBuffer),
                [this, messageBuffer](const boost::system::error_code& ec, std::size_t bytesTransferred){
                    if (!ec){
                        std::string receivedMessage(messageBuffer->begin(), messageBuffer->end());
                        clientLogger_.info("Received: " + receivedMessage);

                        isWorking_ = false;
                        recvLock_.notify_one();

                        asyncReceiveMessage(); // Continue reading
                    }
                    else {
                        clientLogger_.error("Error receiving message: " + ec.message());
                        isWorking_ = false;
                        recvLock_.notify_one();
                    }
                });
            }
            else {
                clientLogger_.error("Error receiving message header: " + ec.message());
                isWorking_ = false;
                recvLock_.notify_one();
            }
        }); 
    }

    void waitUntill(bool &condition, bool req=false){
        std::unique_lock<std::mutex> lock(mtx_);
        recvLock_.wait(lock, [this, &condition, &req](){ 
            return condition==req; 
        });
        lock.unlock();
    }

    // template <typename T, typename T2, std::size_t Ndata, std::size_t Neof>
    // void asyncSendMessage(T (&message)[Ndata], T2 (&userEOF)[Neof]){
    //     if (!isConntected){
    //         clientLogger_.warning("No server connected, message not sent.");
    //         return;
    //     }

    //     socket_.async_send(boost::asio::buffer(message, Ndata), 0, [](){

    //     });
    // }

};

#endif