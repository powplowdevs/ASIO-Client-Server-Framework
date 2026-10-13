#pragma once
#include <utility>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

#include "message_queue.h"
#include "connection.h"
#include "connection_map.h"
#include "timeout_manager.h"
#include "logger.h"

namespace acsf
{
    class Server {
        private:
            // Boost
            boost::asio::io_context io_context_;
            boost::asio::ip::tcp::endpoint endpoint_;
            boost::asio::ip::tcp::acceptor acceptor_;
            Logger logger_;
            
            std::thread workerThread_; 
            MessageQueue messageQueue_;
            TimeoutManager timeoutManager_;
            ConnectionMap conMap_;
            uint64_t currentId_ = 0;
            std::function<void()> callBack_;
            bool keepAliveByHeartBeat; 
            bool sendHeartBeat; 
            

            void startAccepting(){
                acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                    if (!ec) {
                        handleConnection(std::move(socket));
                        startAccepting();
                    }
                    else{
                        if(ec == boost::asio::error::operation_aborted) { return; }
                        Log log;
                        log.level = LogLevel::ERROR;
                        log.message = "Critical error while listening for clients";
                        log.localAddress = endpoint_.address().to_string();
                        log.localPort = endpoint_.port();
                        log.errorValue = ec.value();
                        log.errorMessage = ec.message();
                        logger_.push(std::move(log));
                        // PROLLY ADD REARM
                        std::cout << "Critcal error while listening for clients: " << ec.message() << std::endl;
                    }
                });
            }       

            void handleConnection(boost::asio::ip::tcp::socket socket){
                conMap_.addAndGetConnection(currentId_, std::move(socket), messageQueue_,  timeoutManager_, logger_, [this, id = currentId_](){
                    if(callBack_) { callBack_(); }
                    conMap_.remove(id);
                },
                keepAliveByHeartBeat,
                sendHeartBeat
                )->start();
                currentId_++;
            }

            


        public:

            Server(std::uint16_t port, std::function<void()> callback, bool keepAliveByHeartBeat = true, bool sendHeartBeats = false, LoggerConfig loggerConfig = {}) : 
                endpoint_(boost::asio::ip::tcp::v4(), port), 
                acceptor_(io_context_, endpoint_),
                logger_(io_context_, std::move(loggerConfig)),
                callBack_(callback),
                keepAliveByHeartBeat(keepAliveByHeartBeat),
                sendHeartBeat(sendHeartBeats)
                {}

            ~Server() {
                stop(); 
            }

            void start(){
                startAccepting();

                workerThread_ = std::thread([this]() {
                    if (io_context_.stopped()) { io_context_.restart(); }
                    io_context_.run(); 
                });

            }

            void stop() {
                boost::system::error_code ec;
                acceptor_.close(ec);

                messageQueue_.close();
                conMap_.closeAll(); // Must close so the io_context runs out of work and join returns

                if (workerThread_.joinable()) {
                    workerThread_.join();
                }
                io_context_.stop();
                logger_.unsafeFlushNow();
            }

            std::optional<acsf::Message> receive(){
                return messageQueue_.pop();
            }

            void send(uint64_t id, std::string body){
                auto con = conMap_.getConnection(id);
                if(!con) { return; }
                con->send(std::move(body));
            }

           void setTimeout(TimeoutType type, std::chrono::milliseconds duration) {
                switch (type) {
                    case TimeoutType::Read:
                        READ_TIMEOUT_MS = duration;
                        break;
                    case TimeoutType::ReadBody:
                        READ_BODY_TIMEOUT_MS = duration;
                        break;
                    case TimeoutType::Send:
                        SEND_TIMEOUT_MS = duration;
                        break;
                    case TimeoutType::HeartbeatTimeout:
                        SERVER_HEARTBEAT_TIMEOUT_MS = duration;
                        break;
                    case TimeoutType::HeartbeatRate:
                        HEARTBEAT_RATE = duration;
                        break;
                }
            }

            

    };
}