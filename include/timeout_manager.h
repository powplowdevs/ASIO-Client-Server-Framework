#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <memory>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace acsf
{

    struct TimeoutManager{
        private:
            std::unordered_map<uint64_t, std::unique_ptr<boost::asio::steady_timer>> timerMap_;
            std::uint64_t currentId_ = 0;
            std::mutex tLock;
            
        public:
             std::optional<uint64_t> startAndGetTimer(std::chrono::milliseconds timeout, boost::asio::any_io_executor ex, std::function<void()> callBack){
                if(timeout == std::chrono::milliseconds::zero()){ return std::nullopt; }
                
                {
                    std::scoped_lock<std::mutex> lock(tLock);
                    currentId_++;

                    std::unique_ptr<boost::asio::steady_timer> timer = std::make_unique<boost::asio::steady_timer>(ex, timeout);    
                    timerMap_[currentId_] = std::move(timer);

                    timerMap_[currentId_]->async_wait([this, id = currentId_, callBack = std::move(callBack)](const boost::system::error_code& ec) {
                        if (!ec) {
                            callBack();
                            cancelTimer(id);
                        }
                    });

                    return currentId_;
                }
                
            }

            void cancelTimer(uint64_t id) {
                {
                    std::scoped_lock<std::mutex> lock(tLock); 
                    timerMap_.erase(id);
                   
                }
            }

            void resetTimer(uint64_t id, std::chrono::milliseconds timeout, std::function<void()> callBack){
                {
                    std::scoped_lock<std::mutex> lock(tLock);
                    if(!timerMap_.contains(id)) {return;}
                    timerMap_[id]->cancel();
                    timerMap_[id]->expires_after(timeout);
                    timerMap_[id]->async_wait([this, id, callBack = std::move(callBack)](const boost::system::error_code& ec) {
                        if (!ec) {
                            callBack();
                        }
                    });
                }
            }
    };

}