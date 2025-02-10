#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H

/**
 * @file common_includes.h
 * @brief This file contains includes for dependencies used commonly across the project, 
 *        as well as the definition of the TaskQueue class, which manages a queue of tasks 
 *        that are processed asynchronously in a separate thread.
 * 
 * @short This class is designed to help with concurrent processing of tasks.
 */

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

// TaskQueue struct that manages a queue of tasks, processing them asynchronously in a separate thread.
struct TaskQueue {
public:
    bool isWorking_; ///< Indicates whether the queue is currently processing a task.
    std::condition_variable isWorkingLock_; ///< A condition variable to wait for a tasks completion.

    /// @brief Constructs the TaskQueue and starts the task processing thread.
    TaskQueue() : stop_(false) {
        taskProcessThread_ = std::thread(&TaskQueue::runQueue, this);  ///< Start processing tasks in a separate thread.
    }

    /// @brief Destructor stops the task queue and joins the processing thread.
    ~TaskQueue() {
        {
            std::lock_guard<std::mutex> lock(qLock_);
            stop_ = true;  ///< Set stop flag to true to signal the thread to stop processing.
        }
        hasTask_.notify_one();  ///< Notify the thread to check for the stop flag.
        taskProcessThread_.join();  ///< Wait for the task processing thread to finish.
    }

    /// @brief Adds a new task to the queue to be processed asynchronously.
    /// @param lambda A function (task) to be added to the task queue.
    void addTask(std::function<void()> lambda) {
        {
            std::lock_guard<std::mutex> lock(qLock_);
            queue_.push(lambda);  ///< Add the task (lambda) to the queue.
        }
        hasTask_.notify_one();  ///< Notify the worker thread that a task is available.
    }
    
    /// @brief Stops the task queue and signals the processing thread to terminate.
    void stopQueue(){
        stop_ = true;
    }

private:
    bool stop_;  ///< Flag indicating whether the queue should stop processing.
    std::queue<std::function<void()>> queue_;  ///< Queue of tasks to be processed.
    std::mutex qLock_;  ///< Mutex to synchronize access to the queue.
    std::thread taskProcessThread_;  ///< The thread that processes tasks from the queue.
    std::condition_variable hasTask_;  ///< Condition variable used to wait for tasks to be available.

    /// @brief Main function that processes tasks in the queue.
    void runQueue() {
        while (true) {   
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(qLock_);
                
                if (stop_) break;  ///< Exit the loop if stop flag is set.

                hasTask_.wait(lock, [this]() {
                    return stop_ || !queue_.empty();  ///< Wait for a task or stop signal.
                });

                task = queue_.front();  ///< Get the next task from the front of the queue.
                queue_.pop();  ///< Remove the task from the queue.
            }

            // Lock to modify the `isWorking_` status and wait for task completion.
            std::unique_lock<std::mutex> lock(qLock_);
            isWorking_ = true;  ///< Set `isWorking_` to true to indicate task is in progress.
            task();  ///< Execute the task (lambda).
            isWorkingLock_.wait(lock, [this]() {
                return isWorking_ == false;  ///< Wait for the task to finish before continuing.
            });
        }
    }
};

struct MessageQueue {
public:
    std::any getLastMessage(){
        waitUntilFilled();
        std::any message = queue_[0];
        queue_.erase(queue_.begin());
        return message;
    }
    
    std::any getAllMessages(){
        waitUntilFilled();
        std::vector<std::any> tQueue = queue_;
        queue_.clear();
        return tQueue;
    }

    void waitUntilFilled(){
        std::unique_lock<std::mutex> lock(qLock_);
        hasMessage_.wait(lock, [this]() {
            return !queue_.empty();  
        });
    }

    void add(std::any message){
        queue_.push_back(message);
        hasMessage_.notify_one();
    }
    
private:
    std::vector<std::any> queue_;
    std::mutex qLock_;
    std::thread messageProcessThread_; 
    std::condition_variable hasMessage_;
};

#endif