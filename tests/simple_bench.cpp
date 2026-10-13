// tests/simple_bench.cpp  --  one server, one client, how fast can they go?
//
// The server (own thread) echoes every message straight back.
// The client (main thread) keeps WINDOW messages "in flight": it sends WINDOW messages, then every time a reply
// comes back it sends another one. A bigger window = more work queued = higher throughput, but each message waits longer.
//
// Each message carries the time it was sent. When the echo arrives:  latency = now - time it was sent.
//
// build: g++-13 -std=c++23 -O2 -DNDEBUG -pthread -I../include simple_bench.cpp -o simple_bench
// run:   ./simple_bench

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "server.h"
#include "client.h"

using Clock = std::chrono::steady_clock;

// ---------------- Settings: change these to customize the test ----------------

// Test shape
const std::uint16_t PORT = 15555;
const std::size_t MESSAGE_SIZE = 64;                          // bytes per message (minimum 8, the send time lives in the first 8)
const auto WARMUP = std::chrono::seconds(2);                  // ignored, lets everything get up to speed
const auto MEASURE = std::chrono::seconds(5);                 // how long each run is measured
const std::vector<int> WINDOWS = {1, 4, 16, 64, 256, 1024};   // messages in flight to try

// Heartbeats (tiny "I'm alive" messages sent when a connection is quiet)
const bool SERVER_SENDS_HEARTBEATS    = false;   // server sends heartbeats to the client
const bool SERVER_REQUIRES_HEARTBEATS = false;   // server drops the client if it goes quiet for HEARTBEAT_TIMEOUT
const bool CLIENT_SENDS_HEARTBEATS    = false;   // client sends heartbeats to the server
const bool CLIENT_REQUIRES_HEARTBEATS = false;   // client drops the server if it goes quiet for HEARTBEAT_TIMEOUT
const auto HEARTBEAT_RATE    = std::chrono::milliseconds(2000);   // how often a heartbeat is sent
const auto HEARTBEAT_TIMEOUT = std::chrono::milliseconds(5000);   // how long silence is tolerated before dropping

// Logging
const bool ENABLE_LOGGING = false;               // write logs to LOG_FOLDER/server and LOG_FOLDER/client
const std::string LOG_FOLDER = "LOG_FOLDER";

// ------------------------------------------------------------------------------

static std::uint64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

static std::string makeMessage() {
    std::string msg(MESSAGE_SIZE, 'x');
    std::uint64_t t = nowNs();
    std::memcpy(msg.data(), &t, sizeof(t));        // first 8 bytes = send time
    return msg;
}

struct Result {
    int window = 0;
    double msgsPerSec = 0;
    double p50 = 0, p99 = 0, max = 0;   // microseconds
};

static Result run(acsf::Client& client, int window) {
    // Fill the window
    for (int i = 0; i < window; ++i) client.send(makeMessage());

    std::vector<std::uint32_t> latenciesUs;
    latenciesUs.reserve(2'000'000);

    auto start = Clock::now();
    auto measureStart = start + WARMUP;
    auto end = measureStart + MEASURE;

    while (Clock::now() < end) {
        auto reply = client.receive();
        if (!reply) break;

        std::uint64_t sentNs;
        std::memcpy(&sentNs, reply->payload.data(), sizeof(sentNs));
        std::uint64_t latencyNs = nowNs() - sentNs;

        if (Clock::now() >= measureStart) latenciesUs.push_back(latencyNs / 1000);

        client.send(makeMessage());   // replace the message that just finished
    }

    // Drain the replies still in flight so the next run starts clean
    for (int i = 0; i < window; ++i) client.receive();

    Result r;
    r.window = window;
    r.msgsPerSec = latenciesUs.size() / std::chrono::duration<double>(MEASURE).count();
    std::sort(latenciesUs.begin(), latenciesUs.end());
    if (!latenciesUs.empty()) {
        r.p50 = latenciesUs[latenciesUs.size() / 2];
        r.p99 = latenciesUs[latenciesUs.size() * 99 / 100];
        r.max = latenciesUs.back();
    }
    return r;
}

int main() {
    acsf::setTimeout(acsf::HEARTBEAT_RATE, HEARTBEAT_RATE);
    acsf::setTimeout(acsf::SERVER_HEARTBEAT_TIMEOUT_MS, HEARTBEAT_TIMEOUT);

    // Server: echo everything back
    acsf::Server server(PORT, [] {}, SERVER_REQUIRES_HEARTBEATS, SERVER_SENDS_HEARTBEATS,
                        {.enabled = ENABLE_LOGGING, .folder = LOG_FOLDER + "/server"});
    server.start();

    std::thread echoThread([&server] {
        while (auto msg = server.receive()) {
            server.send(msg->isFrom, std::move(msg->payload));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    acsf::Client client("127.0.0.1", PORT, [] {}, CLIENT_REQUIRES_HEARTBEATS, CLIENT_SENDS_HEARTBEATS,
                        {.enabled = ENABLE_LOGGING, .folder = LOG_FOLDER + "/client"});
    if (auto ec = client.start()) {
        std::printf("could not connect: %s\n", ec.message().c_str());
        return 1;
    }

    std::printf("1 server, 1 client, %zu byte messages, %lds warmup + %lds measured per run\n",
                MESSAGE_SIZE, (long)WARMUP.count(), (long)MEASURE.count());
    std::printf("heartbeats: server sends=%d requires=%d, client sends=%d requires=%d | logging=%d\n\n",
                SERVER_SENDS_HEARTBEATS, SERVER_REQUIRES_HEARTBEATS, CLIENT_SENDS_HEARTBEATS, CLIENT_REQUIRES_HEARTBEATS, ENABLE_LOGGING);
    std::printf("%10s %14s %10s %10s %10s\n", "in flight", "msgs/sec", "p50 us", "p99 us", "max us");

    Result best;
    for (int window : WINDOWS) {
        Result r = run(client, window);
        std::printf("%10d %14.0f %10.0f %10.0f %10.0f\n", r.window, r.msgsPerSec, r.p50, r.p99, r.max);
        std::fflush(stdout);
        if (r.msgsPerSec > best.msgsPerSec) best = r;
    }

    std::printf("\nbest throughput: %.0f msgs/sec (%d in flight), p99 %.0f us\n", best.msgsPerSec, best.window, best.p99);

    client.stop();
    server.stop();
    echoThread.join();
    return 0;
}
