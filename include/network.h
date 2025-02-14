#ifndef NETWORK_H
#define NETWORK_H

#include "common_includes.h"

using boost::asio::ip::tcp;

class Network {
private:
    boost::asio::io_context& io_context_;
    boost::asio::io_context::work idleWork_;
    std::thread networkThread_;

    std::mutex mtx_;
    std::condition_variable clientsLock_;
    std::condition_variable streamLock_;

    std::string debugPath = "./logs/server_log.txt";
    Logger serverLogger_;    

    int connectedClientsCount_ = 0;
    int timeout_ = 30;
    bool acceptingConnections_ = true;
    bool logDebug_ = false;

public:

};

#endif