#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "protocol.h"

namespace acsf
{


    struct MessageQueue{
        private:
            std::queue<acsf::Message> queue;
            std::mutex qLock;
            std::condition_variable qCv;
            bool closed = false; 
        public:
            bool push(acsf::Message msg){
                try{
                    {
                        std::scoped_lock<std::mutex> lock(qLock);
                        if (closed) return false;
                        queue.push(msg);
                    }
                    qCv.notify_one();
                }
                catch(const std::exception& e){
                    // No logger here: MessageQueue has no Logger reference. Callers get false back and can log it.
                    return false;
                }
                return true;
            }

            std::optional<acsf::Message> pop(){
                std::unique_lock<std::mutex> lock(qLock);
                qCv.wait(lock, [this]{return !queue.empty() || closed;});

                if (queue.empty()) { // Handle empty queue when close close is called
                    return std::nullopt;
                }

                acsf::Message msg = std::move(queue.front());
                queue.pop();

                return msg;
            }

            void close(){
                {
                    std::scoped_lock<std::mutex> lock(qLock);
                    closed = true;
                }
                qCv.notify_all();
            }

    };

}