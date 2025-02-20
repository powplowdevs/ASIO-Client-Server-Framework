/**
    * @author Ayoub Mohamed (powplowdevs on GitHub)
*/

#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H

/**
 * @file common_includes.h
 * @brief This file contains the common dependencies and the definition of the TaskQueue 
 *        and MessageQueue classes, which manage asynchronous task processing and message 
 *        handling in separate threads, respectively.
 * 
 * @short Includes common dependencies and manages task and message queues.
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

/**
 * @struct TaskQueue
 * @brief Manages a queue of tasks and processes them asynchronously in a separate thread.
 * 
 * The TaskQueue allows tasks to be added to a queue, which are then processed in a 
 * separate thread. It uses condition variables to manage synchronization between threads.
 */
struct TaskQueue {
public:
    bool isWorking_; ///< Indicates whether the queue is currently processing a task.
    std::condition_variable isWorkingLock_; ///< A condition variable to wait for a task's completion.

    /**
     * @brief Constructs the TaskQueue and starts the task processing thread.
     * 
     * Initializes the task queue and starts a separate thread to process tasks asynchronously.
     */
    TaskQueue() : stop_(false) {
        taskProcessThread_ = std::thread(&TaskQueue::runQueue, this);  ///< Start processing tasks in a separate thread.
    }

    /**
     * @brief Destructor stops the task queue and joins the processing thread.
     * 
     * Signals the task processing thread to stop and waits for it to finish.
     */
    ~TaskQueue() {
        {
            std::lock_guard<std::mutex> lock(qLock_);
            stop_ = true;  ///< Set stop flag to true to signal the thread to stop processing.
        }
        hasTask_.notify_one();  ///< Notify the thread to check for the stop flag.
        taskProcessThread_.join();  ///< Wait for the task processing thread to finish.
    }

    /**
     * @brief Adds a new task to the queue to be processed asynchronously.
     * @param lambda A function (task) to be added to the task queue.
     * 
     * Adds the provided task (lambda) to the queue and notifies the worker thread to process it.
     */
    void addTask(std::function<void()> lambda) {
        {
            std::lock_guard<std::mutex> lock(qLock_);
            queue_.push(lambda);  ///< Add the task (lambda) to the queue.
        }
        hasTask_.notify_one();  ///< Notify the worker thread that a task is available.
    }
    
    /**
     * @brief Stops the task queue and signals the processing thread to terminate.
     * 
     * Sets the stop flag to true and signals the processing thread to exit its loop.
     */
    void stopQueue(){
        stop_ = true;
    }

private:
    bool stop_;  ///< Flag indicating whether the queue should stop processing.
    std::queue<std::function<void()>> queue_;  ///< Queue of tasks to be processed.
    std::mutex qLock_;  ///< Mutex to synchronize access to the queue.
    std::thread taskProcessThread_;  ///< The thread that processes tasks from the queue.
    std::condition_variable hasTask_;  ///< Condition variable used to wait for tasks to be available.

    /**
     * @brief Main function that processes tasks in the queue.
     * 
     * Continuously processes tasks from the queue. It waits for tasks to be added 
     * and processes them in order. The loop terminates when the stop flag is set.
     */
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

/**
 * @struct MessageQueue
 * @brief Manages a queue of messages and processes them asynchronously.
 * 
 * The MessageQueue allows messages to be added to a queue and processed in a separate thread. 
 * It uses a condition variable to synchronize access to the queue.
 */
struct MessageQueue {
public:
    /**
     * @brief Retrieves the last message from the queue.
     * 
     * Waits for the queue to have a message, retrieves and removes the first message from the queue.
     * 
     * @return The first message from the queue.
     */
    std::any getLastMessage(){
        waitUntilFilled();  ///< Wait for the queue to have a message.
        std::any message = queue_[0];  ///< Get the first message.
        queue_.erase(queue_.begin());  ///< Remove the message from the queue.
        return message;
    }
    
    /**
     * @brief Retrieves and clears all messages from the queue.
     * 
     * Waits for the queue to have messages, retrieves all of them, and then clears the queue.
     * 
     * @return A vector containing all the messages in the queue.
     */
    std::any getAllMessages(){
        waitUntilFilled();  ///< Wait for the queue to have messages.
        std::vector<std::any> tQueue = queue_;  ///< Copy all messages.
        queue_.clear();  ///< Clear the queue.
        return tQueue;
    }

    /**
     * @brief Waits until the queue has at least one message.
     * 
     * Blocks the calling thread until there is a message in the queue.
     */
    void waitUntilFilled(){
        std::unique_lock<std::mutex> lock(qLock_);
        hasMessage_.wait(lock, [this]() {
            return !queue_.empty();  ///< Wait until the queue is not empty.
        });
    }

    /**
     * @brief Adds a new message to the queue.
     * @param message The message to be added to the queue.
     * 
     * Adds the provided message to the queue and notifies the worker thread to process it.
     */
    void add(std::any message){
        queue_.push_back(message);  ///< Add the message to the queue.
        hasMessage_.notify_one();  ///< Notify the worker thread that a message is available.
    }
    
private:
    std::vector<std::any> queue_;  ///< Queue of messages to be processed.
    std::mutex qLock_;  ///< Mutex to synchronize access to the queue.
    std::thread messageProcessThread_;  ///< Thread to process messages (not currently used).
    std::condition_variable hasMessage_;  ///< Condition variable used to wait for messages to be available.
};

#endif