#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace acsf
{

    struct OutboxQueue{
        private:
            std::queue<std::shared_ptr<std::vector<char>>> queue;
            std::mutex qLock;
            bool isWriting = false;
            bool closed = false;

            void startWrite(){
                isWriting = true;
            }

            void stopWrite(){
                isWriting = false;
            }

            bool checkWriting(){
                return isWriting;
            }

        public:
            bool pushAndClaim(std::shared_ptr<std::vector<char>> msg){
                {
                    std::scoped_lock<std::mutex> lock(qLock);
                    if(closed) { return false; }

                    queue.push(std::move(msg));
                    if(checkWriting()){ return false; }
                    startWrite();
                    return true;
                }
                return true;
            }

            std::shared_ptr<std::vector<char>> popAndNext(){
                 {
                    std::scoped_lock<std::mutex> lock(qLock);

                    queue.pop();
                    if (queue.empty()) {
                        stopWrite();
                        return nullptr;
                    }

                    return queue.front();  
                }
            }

            void close(){
                {
                    std::scoped_lock<std::mutex> lock(qLock);
                    closed = true;
                }
            }

    };

}