#pragma once
#include <utility>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

#include "message_queue.h"
#include "connection.h"

#include "timeout_manager.h"
#include "logger.h"

namespace acsf
{
    class Client {      
        private:
            // Boost
            boost::asio::io_context io_context_;
            boost::asio::ip::tcp::socket clientSocket_;
            boost::asio::ip::tcp::endpoint endpoint_;
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard_;
            Logger logger_;
            
            std::thread workerThread_; 
            MessageQueue messageQueue_;
            std::shared_ptr<Connection> clientCon_;
            std::atomic_bool isClosed_ = false;
            std::function<void()> callBack_;
            TimeoutManager timeoutManager_;
            bool keepAliveByHeartBeat; 
            bool sendHeartBeat; 


            void log(LogLevel level, std::string message, const boost::system::error_code& ec = {}){
                Log log;
                log.level = level;
                log.message = std::move(message);
                log.remoteAddress = endpoint_.address().to_string();
                log.remotePort = endpoint_.port();
                if(ec){
                    log.errorValue = ec.value();
                    log.errorMessage = ec.message();
                }
                logger_.push(std::move(log));
            }

            void handleConnection(boost::asio::ip::tcp::socket socket, std::function<void()> callback){
                clientCon_ = std::make_shared<Connection>(std::move(socket), 0, messageQueue_, timeoutManager_, logger_, callback, keepAliveByHeartBeat, sendHeartBeat);
                clientCon_->start();
            }

        public:

            Client(std::string ip, std::uint16_t port, std::function<void()> callBack = [](){}, bool keepAliveByHeartBeat = false, bool sendHeartBeats = true, LoggerConfig loggerConfig = {}): 
            clientSocket_(io_context_), 
            endpoint_(boost::asio::ip::make_address(ip), port), 
            workGuard_(boost::asio::make_work_guard(io_context_)),
            logger_(io_context_, std::move(loggerConfig)),
            callBack_(callBack),
            keepAliveByHeartBeat(keepAliveByHeartBeat),
            sendHeartBeat(sendHeartBeats)
            {}
               
            ~Client() {
                stop(); 
            }

            boost::system::error_code start(){
                if(clientCon_) {
                    log(LogLevel::WARN, "start() called while already connected", boost::asio::error::already_connected);
                    return boost::asio::error::already_connected;
                }
                boost::system::error_code ec;
                clientSocket_.connect(endpoint_, ec);
                if(ec) {
                    log(LogLevel::ERROR, "Failed to connect to server", ec);
                    return ec;
                }
                handleConnection(std::move(clientSocket_), [this]{ 
                    messageQueue_.close(); 
                    if (callBack_) callBack_(); 
                }); // Also close msg queue with callback to free waiting recv calls

                workerThread_ = std::thread([this]() {
                    io_context_.run(); 
                });

                return boost::system::error_code();

            }

            void stop(){
                if(isClosed_.exchange(true)) {return;}

                workGuard_.reset();
                messageQueue_.close();

                if(clientCon_) { clientCon_->close(); } // Call back called from connection class

                if (workerThread_.joinable()) {
                    workerThread_.join(); 
                }
                io_context_.stop();
                logger_.unsafeFlushNow();
            }

            std::optional<acsf::Message> receive(){
                return messageQueue_.pop();
            }

            void send(std::string body){
                if(!clientCon_) { return; }
                clientCon_->send(std::move(body));
            }
            

    };
}