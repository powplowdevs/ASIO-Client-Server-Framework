#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <mutex>
#include <filesystem>

enum LogLevel { INFO, WARNING, ERROR, DEBUG };

class Logger {
private:
    static std::mutex mtx_;
    std::string logFilename_;

    void ensureDirectoryExists(){
        std::filesystem::path logPath(logFilename_);
        std::filesystem::path dir = logPath.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)){
            std::filesystem::create_directories(dir);
        }
    }

public:
    Logger(const std::string& logFilename) : logFilename_(logFilename){
        ensureDirectoryExists();
    }

    void log(LogLevel level, const std::string& message){
        std::lock_guard<std::mutex> lock(mtx_);

        std::string levelStr;
        switch (level){
            case INFO: levelStr = "INFO"; break;
            case WARNING: levelStr = "WARNING"; break;
            case ERROR: levelStr = "ERROR"; break;
            case DEBUG: levelStr = "DEBUG"; break;
        }

        std::time_t now = std::time(0);
        std::tm* nowTm = std::localtime(&now);

        char timestamp[20];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", nowTm);

        std::ofstream logFile(logFilename_, std::ios_base::app);
        logFile << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;

        std::cout << "[" << levelStr << "] " << message << std::endl;
    }

    void info(const std::string& message){
        log(INFO, message);
    }

    void warning(const std::string& message){
        log(WARNING, message);
    }

    void error(const std::string& message){
        log(ERROR, message);
    }

    void debug(const std::string& message){
        log(DEBUG, message);
    }
};

std::mutex Logger::mtx_;

#endif