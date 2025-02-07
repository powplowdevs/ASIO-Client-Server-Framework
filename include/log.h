#ifndef LOG_H
#define LOG_H
/**
 * @file log.h
 * @brief This file defines the Logger class used for logging messages at various levels.
 *        It allows logging to both a log file and the console with timestamps and log levels 
 *        (INFO, WARNING, ERROR, DEBUG).
 */

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <mutex>
#include <filesystem>

// Enum representing different log levels.
enum LogLevel { INFO, WARNING, ERROR, DEBUG };

class Logger {
private:
    static std::mutex mtx_;  ///< Mutex to ensure thread safety when logging.
    std::string logFilename_;  ///< The name of the log file to write logs to.

    /// @brief Ensures that the directory for the log file exists. 
    ///        Creates the directory if it does not exist.
    void ensureDirectoryExists(){
        std::filesystem::path logPath(logFilename_);
        std::filesystem::path dir = logPath.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)){
            std::filesystem::create_directories(dir);  ///< Create the necessary directories.
        }
    }

public:
    /// @brief Constructs a Logger instance.
    /// @param logFilename Name of the log file to which logs will be written.
    Logger(const std::string& logFilename) : logFilename_(logFilename){
        ensureDirectoryExists();  ///< Ensure the log directory exists upon construction.
    }

    /// @brief Logs a message to the console and appends it to the log file.
    /// @param level The log level indicating the level of the log.
    /// @param message The log message to be recorded.
    void log(LogLevel level, const std::string& message){
        std::lock_guard<std::mutex> lock(mtx_);  ///< Locking the mutex to ensure thread safety.

        // Convert log level to string representation.
        std::string levelStr;
        switch (level){
            case INFO: levelStr = "INFO"; break;
            case WARNING: levelStr = "WARNING"; break;
            case ERROR: levelStr = "ERROR"; break;
            case DEBUG: levelStr = "DEBUG"; break;
        }

        // Get the current timestamp.
        std::time_t now = std::time(0);
        std::tm* nowTm = std::localtime(&now);

        char timestamp[20];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", nowTm);  ///< Format the timestamp.

        // Open the log file in append mode and write the log entry.
        std::ofstream logFile(logFilename_, std::ios_base::app);
        logFile << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;

        // Also print the log message to the console for immediate feedback.
        std::cout << "[" << levelStr << "] " << message << std::endl;
    }

    /// @brief Logs an informational message.
    /// @param message The message to log.
    void info(const std::string& message){
        log(INFO, message);  ///< Call the main log function with INFO level.
    }

    /// @brief Logs a warning message.
    /// @param message The message to log.
    void warning(const std::string& message){
        log(WARNING, message);  ///< Call the main log function with WARNING level.
    }

    /// @brief Logs an error message.
    /// @param message The message to log.
    void error(const std::string& message){
        log(ERROR, message);  ///< Call the main log function with ERROR level.
    }

    /// @brief Logs a debug message.
    /// @param message The message to log.
    void debug(const std::string& message){
        log(DEBUG, message);  ///< Call the main log function with DEBUG level.
    }
};

// Initialize the static mutex to be used by all instances of the Logger class.
std::mutex Logger::mtx_;

#endif