// include/protocol.h
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <bit>
#include <vector>
#include <cstring>
#include <string_view>
#include <optional>
#include <mutex>

namespace acsf {

inline constexpr std::size_t HeaderSize = 4;
inline constexpr std::uint32_t MaxMessageSize = 1024 * 1024;        // 1 MB

// Logger
inline std::uintmax_t MAX_LOG_FILE_SIZE = 10 * 1024 * 1024;
inline constexpr std::uintmax_t LOG_BUFFER_SIZE = 1024; 

// Timeouts
inline std::mutex pLock;
//                                timeout on any read  timeout on read after header  timeout on any send    timeout on client sending data
inline std::chrono::milliseconds READ_TIMEOUT_MS{15000}, READ_BODY_TIMEOUT_MS{60000}, SEND_TIMEOUT_MS{60000}, SERVER_HEARTBEAT_TIMEOUT_MS{45000}, HEARTBEAT_RATE{15000};


struct Message {
    uint64_t isFrom = 0;
    bool isHeartBeat = false;
    std::string payload;
};

enum class TimeoutType {
    Read,
    ReadBody,
    Send,
    HeartbeatTimeout,
    HeartbeatRate
};

// Logger
inline void setMaxLogFileSize(std::uintmax_t newSize){
    MAX_LOG_FILE_SIZE = newSize;
}

// Timeouts
inline void setTimeout(std::chrono::milliseconds &targetTimeout, std::chrono::milliseconds timeout){
    {
        std::scoped_lock<std::mutex> lock(pLock);
        targetTimeout = timeout;
    }
}

// CPU = little endian : Netowrk = Big endian
inline std::uint32_t networkToHostLong(uint32_t bytes){
    // Big endian -> system endian
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(bytes);
    }
    return bytes;
}

inline std::uint32_t hostToNetworkLong(uint32_t bytes){
    // system endian -> big endian
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(bytes);
    }
    return bytes;
}

// Builds [4-byte big-endian length][body].
inline std::optional<std::vector<char>> getMessageFrame(std::string_view body, bool isHeartBeat){
    if (body.size() > MaxMessageSize || (body.size() <= 0 && !isHeartBeat)) { 
        // No logger here: protocol functions are free functions with no Logger. The caller (Connection) logs the nullopt.
        return std::nullopt;
    }
    
    std::vector<char> messageFrame;

    uint32_t bodySize = body.size();
    uint32_t header = bodySize | (isHeartBeat ? (1u << 31) : 0u);
    uint32_t netPacket = hostToNetworkLong(header);

    messageFrame.resize(sizeof(netPacket) + bodySize);

    std::memcpy(messageFrame.data(), &netPacket, sizeof(netPacket));
    std::memcpy(messageFrame.data() + sizeof(netPacket), body.data(), bodySize);

    return messageFrame;
}

// Reads the 4 header bytes and returns the payload length and heart beat bool.
inline std::optional<std::pair<std::uint32_t, bool>> parseHeader(const std::array<char, HeaderSize>& header){ 
    std::uint32_t data = 0;   
    std::uint32_t length = 0;   
    bool isHeartbeat;

    std::memcpy(&data, header.data(), HeaderSize);
    data = networkToHostLong(data);

    length = (data << 1) >> 1;
    isHeartbeat = data & (1u << 31);

    if (length > MaxMessageSize || (length <= 0 && !isHeartbeat)) { 
        // No logger here: protocol functions are free functions with no Logger. The caller (Connection) logs the nullopt.
        return std::nullopt;
    }

    return std::make_pair(length, isHeartbeat);
}

}