#pragma once
#include <utility>
#include <boost/asio.hpp>
#include <iostream>
#include <atomic>
#include <memory>

#include "protocol.h"
#include "message_queue.h"
#include "outbox_queue.h"
#include "timeout_manager.h"
#include "logger.h"

namespace acsf
{
    class Connection : public std::enable_shared_from_this<Connection> {
    private:
        boost::asio::ip::tcp::socket socket_;
        std::uint64_t id_;
        bool closed_ = false;

        MessageQueue& messageQueue_;
        OutboxQueue outboxQueue_;
        TimeoutManager& timeoutManager_;
        Logger& logger_;

        std::array<char, HeaderSize> headerBuf_;
        std::vector<char> bodyBuf_;
        std::function<void()> callback_;
        std::optional<std::uint64_t> heartBeatTimeoutTimerID_; // Timeout for peer not sending heartbeats
        std::optional<std::uint64_t> heartBeatSendTimerID_;    // Timer driving our own heartbeat sends
        bool keepAliveByHeartBeat; 
        bool sendHeartBeat; 

        std::optional<std::string> remoteAddress_;
        std::optional<uint16_t> remotePort_;

        //boost::asio::steady_timer readBodyTimeoutTimer_, readTimeoutTimer_, sendTimeoutTimer_, heartBeatTimeoutTimer_;
        //uint64_t readBodyTimeout_ = 0, readTimeout_ = 0, sendTimeout = 0, heartBeatTimeout = 0;

        void log(LogLevel level, std::string message, const boost::system::error_code& ec = {}){
            Log log;
            log.isFrom = id_;
            log.level = level;
            log.message = std::move(message);
            log.remoteAddress = remoteAddress_;
            log.remotePort = remotePort_;
            if(ec){
                log.errorValue = ec.value();
                log.errorMessage = ec.message();
            }
            logger_.push(std::move(log));
        }

        void doRead(){
            auto self = shared_from_this();

            auto readTimeoutID = timeoutManager_.startAndGetTimer(READ_TIMEOUT_MS, socket_.get_executor(), [this, self](){ boost::system::error_code ignored; socket_.cancel(ignored); });

            boost::asio::async_read(socket_, boost::asio::buffer(headerBuf_), // read header
            [this, self, readTimeoutID](const boost::system::error_code& ec, std::size_t) {   
                if(readTimeoutID) { timeoutManager_.cancelTimer(*readTimeoutID); } 
                if(!ec){

                    // Hearbeat reset
                    if(heartBeatTimeoutTimerID_ && keepAliveByHeartBeat){
                        timeoutManager_.resetTimer(*heartBeatTimeoutTimerID_, SERVER_HEARTBEAT_TIMEOUT_MS, [this, self](){
                            log(LogLevel::WARN, "Heartbeat timeout, closing connection");
                            close();
                        });
                    }

                    auto parseResult = parseHeader(headerBuf_);
                     if(!parseResult){
                        log(LogLevel::ERROR, "Invalid message header received, closing connection");
                        close();
                        return;
                    }
                    
                    auto [bodyLength, isHeartBeat] = *parseResult;

                    bodyBuf_.resize(bodyLength);

                    auto readBodyTimeoutID = timeoutManager_.startAndGetTimer(READ_BODY_TIMEOUT_MS, socket_.get_executor(), [this, self](){ boost::system::error_code ignored; socket_.cancel(ignored); });
                    
                    boost::asio::async_read(socket_, boost::asio::buffer(bodyBuf_), // Read body
                    [this, self, readBodyTimeoutID, isHeartBeat](const boost::system::error_code& ec, std::size_t) {  
                        if(readBodyTimeoutID) { timeoutManager_.cancelTimer(*readBodyTimeoutID); }   
                        if(!ec){
                            
                            if(!isHeartBeat){ 
                                acsf::Message msg = {id_, isHeartBeat, {bodyBuf_.begin(), bodyBuf_.end()}};
                                messageQueue_.push(std::move(msg));
                            }

                            doRead();
                        }
                        else{
                            if(closed_) { return; } // We closed it somewhere so its not an error
                            if(ec == boost::asio::error::operation_aborted) {
                                log(LogLevel::WARN, "Reading body timed out", ec);
                            }
                            else if(ec == boost::asio::error::eof) {
                                log(LogLevel::INFO, "Connection ended while reading body", ec);
                            }
                            else {
                                log(LogLevel::ERROR, "Error in reading body", ec);
                            }

                            close();
                            return;
                        }
                    });
                }
                else{
                    if(closed_) { return; }
                    if(ec == boost::asio::error::operation_aborted) {
                        log(LogLevel::WARN, "Reading header timed out", ec);
                    }
                    else if(ec == boost::asio::error::eof) {
                        log(LogLevel::INFO, "Connection ended while reading header", ec);
                    }
                    else {
                        log(LogLevel::ERROR, "Error in reading header", ec);
                    }

                    close();
                    return;
                }

            });
        }


        void doSend(std::shared_ptr<std::vector<char>> body){
            auto self = shared_from_this();

            auto sendTimeoutID = timeoutManager_.startAndGetTimer(SEND_TIMEOUT_MS, socket_.get_executor(), [this, self](){ boost::system::error_code ignored; socket_.cancel(ignored); });

            boost::asio::async_write(socket_, boost::asio::buffer(*body), 
            [this, self, body, sendTimeoutID](const boost::system::error_code& ec, std::size_t) {
                if(sendTimeoutID) { timeoutManager_.cancelTimer(*sendTimeoutID); }
                if (!ec) {
                    std::shared_ptr<std::vector<char>> nextMsg = outboxQueue_.popAndNext();
                    if(nextMsg) { 
                        doSend(std::move(nextMsg)); 
                    }
                    else{
                        return;
                    }
                } 
                else {
                    if(closed_) { return; }
                    if(ec == boost::asio::error::operation_aborted) {
                        log(LogLevel::WARN, "Sending timed out", ec);
                    }
                    else if(ec == boost::asio::error::eof) {
                        log(LogLevel::INFO, "Connection ended while sending", ec);
                    }
                    else {
                        log(LogLevel::ERROR, "Error in sending data", ec);
                    }

                    close();
                    return;
                }
            });
            
          
        }

        // Make first heatbeat timer
        // set callback to send and doHeartBeatSend
        // Timer ends and send is called then doHeartBeatSend
        // DoHeatBeatSend checks if timer exists
        // DoHeartBeatSend resets said timer with call back of send and doHeartBeatSend(ID)

        void doHeartBeatLoop(uint64_t id){
            auto self = shared_from_this();
            timeoutManager_.resetTimer(id, HEARTBEAT_RATE, [self, id](){
                if(self->closed_) { return; } // Stop the loop (and release self) once the connection is closed
                self->send("", true);
                self->doHeartBeatLoop(id);
            });
        }

    public:
        // Constructor  
        Connection(boost::asio::ip::tcp::socket&& socket, std::uint64_t id, MessageQueue& messageQueue, TimeoutManager& timeoutManager, Logger& logger, std::function<void()> cb_, bool keepAliveByHeartBeat = true, bool sendHeartBeats = true) 
        : socket_(std::move(socket)), id_(id), messageQueue_(messageQueue),  timeoutManager_(timeoutManager), logger_(logger), callback_(std::move(cb_)), keepAliveByHeartBeat(keepAliveByHeartBeat), sendHeartBeat(sendHeartBeats)
        {
            boost::system::error_code ec;
            auto remote = socket_.remote_endpoint(ec);
            if(!ec){
                remoteAddress_ = remote.address().to_string();
                remotePort_ = remote.port();
            }

            socket_.set_option(boost::asio::ip::tcp::no_delay(true), ec);
            if(ec){
                log(LogLevel::WARN, "Failed to set TCP_NODELAY", ec);
            }
        }

        void start(){
            auto self = shared_from_this();
            if(SERVER_HEARTBEAT_TIMEOUT_MS != std::chrono::milliseconds::zero() && keepAliveByHeartBeat){
                heartBeatTimeoutTimerID_ = timeoutManager_.startAndGetTimer(SERVER_HEARTBEAT_TIMEOUT_MS, socket_.get_executor(), [this, self](){
                    log(LogLevel::WARN, "Heartbeat timeout, closing connection");
                    close();
                });
            }

            if(sendHeartBeat){
                heartBeatSendTimerID_ = timeoutManager_.startAndGetTimer(HEARTBEAT_RATE, socket_.get_executor(), [](){});
                if(heartBeatSendTimerID_) { doHeartBeatLoop(*heartBeatSendTimerID_); }
            }

            doRead();
        }   
        
        void send(std::string body, bool isHeartBeat = false){    
            auto msgFrame = getMessageFrame(body, isHeartBeat);
            if(!msgFrame){
                log(LogLevel::WARN, "Failed to build message frame (body empty or larger than MaxMessageSize), size=" + std::to_string(body.size()));
                return;
            }

            auto msg = std::make_shared<std::vector<char>>(std::move(*msgFrame));

            auto self = shared_from_this();
            boost::asio::post(socket_.get_executor(), [this, self, msg]() {
                if(closed_) { return; } 
                if(outboxQueue_.pushAndClaim(msg)){ // if outbox is busy just queue msgframe
                    doSend(msg);
                }
            });
        }

        void close(){
            auto self = shared_from_this();
            
            boost::asio::post(socket_.get_executor(), [this, self]() {
                if (closed_) return;
                closed_ = true;

                // Kill heartbeat timers
                if(heartBeatTimeoutTimerID_) { timeoutManager_.cancelTimer(*heartBeatTimeoutTimerID_); heartBeatTimeoutTimerID_.reset(); }
                if(heartBeatSendTimerID_) { timeoutManager_.cancelTimer(*heartBeatSendTimerID_); heartBeatSendTimerID_.reset(); }

                outboxQueue_.close();

                if (socket_.is_open()) {
                    boost::system::error_code ec;
                    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    socket_.close(ec);
                }
                if(callback_) {callback_();}
            });
        }

    };

}