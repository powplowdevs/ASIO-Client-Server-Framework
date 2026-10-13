#pragma once
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <format>
#include <cstdint>
#include <optional>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>

#include "protocol.h"

namespace acsf {

    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };


    inline std::string levelToString(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
        }
        return "UNKNOWN";
    }

    struct Log
    {
        // General
        std::optional<uint64_t> isFrom;
        std::optional<LogLevel> level;
        std::optional<std::chrono::system_clock::time_point> timestamp;
        std::optional<std::string> message;          // human readable message / context

        // boost::system::error_code details
        std::optional<int> errorValue;               // ec.value()
        std::optional<std::string> errorMessage;     // ec.message()

        // Endpoint info
        std::optional<std::string> localAddress;
        std::optional<uint16_t> localPort;
        std::optional<std::string> remoteAddress;
        std::optional<uint16_t> remotePort;

        std::string toString() const
        {
            std::string result;

            if (timestamp.has_value())
                result += std::format("[{:%Y-%m-%d %H:%M:%S}] ", std::chrono::floor<std::chrono::milliseconds>(timestamp.value()));

            if (level.has_value())
                result += "[" + levelToString(level.value()) + "] ";

            if (isFrom.has_value())
                result += "[from=" + std::to_string(isFrom.value()) + "] ";

            if (message.has_value())
                result += message.value();

            if (remoteAddress.has_value())
                result += " remote=" + remoteAddress.value();

            if (remotePort.has_value())
                result += ":" + std::to_string(remotePort.value());

            if (errorValue.has_value())
                result += " [error " + std::to_string(errorValue.value()) + "]";

            if (errorMessage.has_value())
                result += " " + errorMessage.value();

            return result;
        }
    };
    
    
    struct LoggerConfig
    {
        bool enabled = false;

        std::filesystem::path folder = "LOG_FOLDER";

        std::uintmax_t maxFileSize = MAX_LOG_FILE_SIZE;
    };


    class Logger {
        private:
            LoggerConfig config_;
            std::ofstream CURRENT_LOG_FILE_;
            std::filesystem::path LOG_FOLDER_PATH_;
            boost::asio::strand<boost::asio::io_context::executor_type> loggerStrand_;
            boost::lockfree::spsc_queue<Log, boost::lockfree::capacity<LOG_BUFFER_SIZE>> logBuffer_;

            std::filesystem::path newLogFilePath() const {
                std::string name = std::format("LOG_FILE-{:%Y-%m-%d_%H-%M-%S}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));

                // Prevent any possiblity of dupe file
                std::filesystem::path path = LOG_FOLDER_PATH_ / (name + ".log");
                for(int i = 1; std::filesystem::exists(path); ++i){
                    path = LOG_FOLDER_PATH_ / (name + "-" + std::to_string(i) + ".log");
                }
                return path;
            }

            void rotateLogFile(){
                std::ofstream newLogFile(newLogFilePath());
                if(!newLogFile.is_open()) {
                    return;
                }

                CURRENT_LOG_FILE_.close();
                CURRENT_LOG_FILE_ = std::move(newLogFile);
            }

            void unsafeFlush(bool checkSize = false){
                if(!CURRENT_LOG_FILE_.is_open()) return;

                // Write buffer to file
                Log log;
                while(logBuffer_.pop(log)){
                    CURRENT_LOG_FILE_ << log.toString() << '\n';
                }
                CURRENT_LOG_FILE_.flush();

                // Check if file is max size
                if(checkSize && static_cast<std::uintmax_t>(CURRENT_LOG_FILE_.tellp()) >= config_.maxFileSize){
                    rotateLogFile();
                }
            }

            void unsafePushLog(Log log){
                if(!log.timestamp.has_value())
                    log.timestamp = std::chrono::system_clock::now();

                if(!logBuffer_.push(log)){
                    unsafeFlush(true);
                    logBuffer_.push(std::move(log));
                }
            }

        public:

            Logger(boost::asio::io_context& ioc, LoggerConfig config = {}) : config_(std::move(config)), LOG_FOLDER_PATH_(config_.folder), loggerStrand_(ioc.get_executor()) {
                if(!config_.enabled) return;

                if(LOG_FOLDER_PATH_.empty()){
                    LOG_FOLDER_PATH_ = "LOG_FOLDER";
                }
                std::filesystem::create_directories(LOG_FOLDER_PATH_);
                LOG_FOLDER_PATH_ = std::filesystem::absolute(LOG_FOLDER_PATH_);

                CURRENT_LOG_FILE_.open(newLogFilePath());
            }

            ~Logger(){
                unsafeFlush(false);
            }

            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;

            void unsafeFlushNow(){
                unsafeFlush(true);
            }

            void flush(){
                boost::asio::post(loggerStrand_, [this](){unsafeFlush(true);});
            }

            void push(Log log){
                if(!config_.enabled) return;

                boost::asio::post(loggerStrand_, [this, log = std::move(log)]() mutable {unsafePushLog(std::move(log));});
            }
    };
}
