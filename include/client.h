/**
    * @author Ayoub Mohamed (powplowdevs on GitHub)
*/

#ifndef CLIENT_H
#define CLIENT_H

/**
 * @file client.h
 * @brief Header file that defines the Client class responsible for managing client-side operations including connection, message sending/receiving, and client lifecycle management.
*/

#include "common_includes.h"

using boost::asio::ip::tcp;

/**
 * @class Client
 * @brief Represents a client that connects to a server, sends and receives messages asynchronously.
 * 
 * This class manages the connection to the server, handles sending and receiving messages 
 * asynchronously using Boost.Asio, and provides a task queue to handle these operations 
 * in the background.
 */
class Client {
private:
    boost::asio::io_context& io_context_; ///< The IO context used for asynchronous operations.
    boost::asio::io_context::work idleWork_; ///< Keeps the IO context running.
    std::thread clientThread_; ///< The thread running the IO context.

    std::mutex mtx_; ///< Mutex for synchronization.
    std::condition_variable recvLock_; ///< Condition variable for message reception.
    TaskQueue queue_; ///< TaskQueue used for background task management.
    MessageQueue msgQueue_; ///< MessageQueue used to store and manage messages.

    std::string debugPath = "./logs/client_log.txt"; ///< Path to the log file.
    Logger clientLogger_; ///< Logger for the client.

    tcp::socket socket_; ///< Socket used for communication with the server.
    tcp::resolver resolver_; ///< Resolver to resolve the server IP and port.
    std::string receivedMessage_; ///< Stores the received message.
    tcp::resolver::iterator endpoint_iterator; ///< Iterator for resolving the endpoint.

    std::string serverIp_; ///< The IP address of the server.
    int serverPort_; ///< The port to connect to on the server.

    bool isConnected_ = false; ///< Whether the client is connected to the server.
    int timeout_ = 30; ///< Timeout duration for asynchronous operations.
    bool logDebug_ = false; ///< Flag to enable or disable debug logging.

public:
    /**
     * @brief Constructs the Client object and initializes the connection parameters.
     * @param io_context The IO context for asynchronous operations.
     * @param server_ip The server's IP address.
     * @param server_port The server's port number.
     */
    Client(boost::asio::io_context& io_context, const std::string& server_ip, unsigned short server_port) 
        : io_context_(io_context), socket_(io_context), resolver_(io_context), 
          endpoint_iterator(resolver_.resolve(server_ip, std::to_string(server_port))), 
          idleWork_(io_context_), serverIp_(server_ip), serverPort_(server_port), clientLogger_(debugPath){
        //...
    }

    /**
     * @brief Starts the client by launching the IO context in a separate thread.
     * 
     * This function initiates the IO context in a new thread, allowing asynchronous operations 
     * to run in the background.
     */
    void start(){
        clientLogger_.info("Starting client");
        clientThread_ = std::thread([this](){
            io_context_.run();
            if(logDebug_) clientLogger_.debug("Server IO context has started");
        });  
        clientThread_.detach();
        if(logDebug_) clientLogger_.debug("Server thread has detached");
    }

    /**
     * @brief Forces the client to shut down, stopping the IO context and joining the client thread.
     * 
     * This method forces a shutdown of the client, also ensuring that the client thread is properly 
     * joined and the IO context is stopped.
     */
    void forceShutdown(){
        clientLogger_.warning("Force shutting down the client...");
        isConnected_ = false;
        if (clientThread_.joinable()){
            clientThread_.join();
        }
        io_context_.stop();
        queue_.stopQueue();
        clientLogger_.info("Client shutdown completed.");
        queue_.isWorking_ = false;
    }

    /**
     * @brief Gracefully shuts down the client, optionally running remaining tasks before shutdown.
     * @param runRemainingTasks If true, remaining tasks are completed before shutting down.
     */
    void shutdown(bool runRemainingTasks=false){
        std::function<void()> shutOffTask = [this](){
            clientLogger_.info("Shutting down the client...");
            
            socket_.close();
            isConnected_ = false;
            io_context_.stop();
            queue_.stopQueue();

            clientLogger_.info("Client shutdown completed.");
            queue_.isWorking_ = false;
        };

        if(!runRemainingTasks){
            shutOffTask();
        }
        else{
            if(logDebug_) clientLogger_.debug("Client shutoff task has been queued");
            queue_.addTask([shutOffTask](){ shutOffTask(); });
        }
    }

    /**
     * @brief Connects the client to the server.
     * 
     * This method attempts to establish a connection to the server using the provided IP and port.
     * It logs whether the connection was successful or failed.
     */
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

    // ~~~~~~~~ Message Functions ~~~~~~~~ //

    /**
     * @brief Asynchronously receives a message from the server.
     * 
     * This method initiates an asynchronous operation to receive a message from the server. It 
     * reads the message header first, then reads the message body based on the header size.
     */
    void asyncReceiveMessage(){
        queue_.isWorking_ = false;
        queue_.isWorkingLock_.notify_one();

        if(!isConnected_) {
            clientLogger_.warning("No server connected, message not received");
            return;
        }

        queue_.addTask([this](){
            auto headerBuffer = std::make_shared<std::array<char, 4>>(); // 4 byte header

            boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
            timer.async_wait([this](const boost::system::error_code& ec){
                if(ec != boost::asio::error::operation_aborted){
                    if(!ec){
                        clientLogger_.error("Client timeout");
                    }
                    else{
                        clientLogger_.error("Timeout timer error " + ec.message());
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
                    if(logDebug_) clientLogger_.debug("Received header: " + std::to_string(messageSize));
                    
                    // Read message body
                    boost::asio::async_read(socket_, boost::asio::buffer(*messageBuffer),
                    [this, messageBuffer, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred){
                        if (!ec){
                            std::string receivedMessage(messageBuffer->begin(), messageBuffer->end());
                            if(logDebug_) clientLogger_.debug("Received: " + receivedMessage);
                            msgQueue_.add(receivedMessage);

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

    /**
     * @brief Asynchronously sends a message to the server.
     * @tparam T The type of the message (array of data).
     * @param message The message to be sent.
     * 
     * This method sends a message to the server asynchronously, including a 4-byte header 
     * that indicates the size of the message.
     */
    template <typename T, std::size_t Ndata>
    void asyncSendMessage(T (&message)[Ndata]){
        if (!isConnected_){
            clientLogger_.warning("No server connected, message not sent.");
            return;
        }

        queue_.addTask([this, &message](){
            uint32_t messageSize = static_cast<uint32_t>(Ndata); // 4 byte header
            messageSize = htonl(messageSize);
            
            boost::asio::steady_timer timer(io_context_, boost::asio::chrono::seconds(timeout_));
            timer.async_wait([this](const boost::system::error_code& ec){
                if(ec != boost::asio::error::operation_aborted){
                    if(!ec){
                        clientLogger_.error("Server timeout");
                    }
                    else{
                        clientLogger_.error("Timeout timer error " + ec.message());
                    }

                    queue_.isWorking_ = false;
                    queue_.isWorkingLock_.notify_one();
                }
            });

            // Send header
            socket_.async_send(boost::asio::buffer(&messageSize, sizeof(messageSize)), 
            [this, &timer, message](const boost::system::error_code& ec, std::size_t bytesTransferred){
                // Send message body
                if (!ec){
                    if(logDebug_) clientLogger_.debug("Header sent: " + std::to_string(bytesTransferred) + " bytes");
                
                    socket_.async_send(boost::asio::buffer(message, Ndata),
                    [this, &timer](const boost::system::error_code& ec, std::size_t bytesTransferred){
                        if (!ec) {
                            if(logDebug_) clientLogger_.debug("Message sent: " + std::to_string(bytesTransferred) + " bytes");
                        } 
                        else {
                            clientLogger_.error("Error sending message: " + ec.message());
                        }
                        timer.cancel();
                        queue_.isWorking_ = false;
                        queue_.isWorkingLock_.notify_one();
                    });
                } 
                else {
                    clientLogger_.error("Error sending header: " + ec.message());
                    timer.cancel();
                    queue_.isWorking_ = false;
                    queue_.isWorkingLock_.notify_one();
                }
            });
        });
    }

    // ~~~~~~~~ Message Functions ~~~~~~~~ //
    
    /**
     * @brief Waits until a condition is met.
     * @param condition The condition to be checked.
     * @param req The expected value of the condition (default is false).
     * 
     * This method waits for the specified condition to be either true or false before proceeding.
     */
    void waitUntill(bool &condition, bool req=false){
        std::unique_lock<std::mutex> lock(mtx_);
        recvLock_.wait(lock, [this, &condition, &req](){ 
            return condition == req; 
        });
        lock.unlock();
    }

    // ~~~~~~~~ Getters ~~~~~~~~ //

    /**
     * @brief Retrieves the last received message.
     * @return The last message from the message queue.
     */
    std::any getLastMessage(){
        return msgQueue_.getLastMessage();
    }

    /**
     * @brief Retrieves all messages in the queue.
     * @return A collection of all messages in the queue.
     */
    std::any getAllMessages(){
        return msgQueue_.getAllMessages();
    }

    // ~~~~~~~~ Setters ~~~~~~~~ //

    /**
     * @brief Enables or disables debug logging.
     * @param value If true, debug logging is enabled; otherwise, it is disabled.
     */
    void setlogDebug_(bool value){
        logDebug_ = value;
    }
};

#endif