#pragma once
#include <queue>
#include <mutex>

#include "connection.h"

namespace acsf
{
    
    struct ConnectionMap
    {
        private:
            std::unordered_map<uint64_t, std::shared_ptr<acsf::Connection>> conMap_;
            std::mutex mLock;
        
        public:
            std::shared_ptr<acsf::Connection> addAndGetConnection(uint64_t id, boost::asio::ip::tcp::socket socket, MessageQueue& messageQueue, TimeoutManager& timeoutManager, Logger& logger, std::function<void()> callback_, bool keepAliveByHeartBeat = true, bool sendHeartBeats = true){
                {
                    std::scoped_lock<std::mutex> lock(mLock);

                    conMap_[id] = std::make_shared<acsf::Connection>(std::move(socket), id, messageQueue, timeoutManager, logger, callback_, keepAliveByHeartBeat, sendHeartBeats);

                    return conMap_[id];
                    
                }
            }

            std::shared_ptr<acsf::Connection> getConnection(uint64_t id){
                {
                    std::scoped_lock<std::mutex> lock(mLock);

                    auto con = conMap_.find(id);
                    if (con != conMap_.end()) {
                        return con->second; 
                    }
                    return nullptr;
                    
                }
            }

            void remove(uint64_t id){
                   {
                    std::scoped_lock<std::mutex> lock(mLock);
                    conMap_.erase(id);
                }
            }

            void closeAll(){
                std::unordered_map<uint64_t, std::shared_ptr<acsf::Connection>> connections;
                {
                    std::scoped_lock<std::mutex> lock(mLock);
                    connections.swap(conMap_);
                }
                for(auto& [id, con] : connections) { con->close(); }
            }

            void clear(){
                {
                    std::scoped_lock<std::mutex> lock(mLock);
                    conMap_ = std::unordered_map<uint64_t, std::shared_ptr<acsf::Connection>>();
                }
            }
    };
}