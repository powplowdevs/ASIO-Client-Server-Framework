#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <thread>
#include <any>
#include <functional>
#include <queue>

#include "log.h"

struct TaskQueue{
public:
    bool isWorking_;
    std::condition_variable isWorkingLock_;
    
    TaskQueue() : stop_(false){
        taskProcessThread_ = std::thread(&TaskQueue::runQueue, this);
    }
    ~TaskQueue() {
        {
            std::lock_guard<std::mutex> lock(qLock_);
            stop_ = true;
        }
        hasTask_.notify_one();
        taskProcessThread_.join();
    }
    
    void addTask(std::function<void()> lambda){
        {
            std::lock_guard<std::mutex> lock(qLock_);
            queue_.push(lambda);
            std::cout << "Adding a task!" << std::endl;
        }
        hasTask_.notify_one(); 
    }
private:

    bool stop_;
    std::queue<std::function<void()>> queue_;
    std::mutex qLock_;
    std::thread taskProcessThread_;
    std::condition_variable hasTask_;
    

    void runQueue() {
        while (true) {   
            std::function<void()> task; 
            {
                std::unique_lock<std::mutex> lock(qLock_);
                hasTask_.wait(lock, [this](){
                    std::cout << "waiting to process task..." << std::endl;
                    return stop_ || !queue_.empty();
                });
                if(stop_) break;
            
                std::cout << "RUNNING TASK" << std::endl;

                task = queue_.front();
                queue_.pop();
            }

            std::unique_lock<std::mutex> lock(qLock_);
            isWorking_ = true;
            task();
            isWorkingLock_.wait(lock, [this](){
                std::cout << "waiting to finish task..." << std::endl;
                return isWorking_ == false;
            });
            std::cout << "TASK DONE" << std::endl;
        }
    }
};

#endif