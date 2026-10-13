// src/examples/main.cpp  --  a basic run-through of the framework
//
// Starts a server that echoes messages back, connects a client to it, sends a few messages,
// prints the replies, then shuts both down. It also shows how to set timeouts, heartbeats and logging.
//
// build (from the src/examples folder): g++-13 -std=c++23 -pthread -I../../include main.cpp -o example
// run:                                  ./example

#include <chrono>
#include <iostream>
#include <thread>

#include "server.h"
#include "client.h"

int main() {
    const std::uint16_t port = 15555;

    // ---- 1. Timeouts ----
    // All timeouts are in milliseconds, and 0 turns that timeout off. They are shared by every server and client in the program.
    acsf::setTimeout(acsf::READ_TIMEOUT_MS, std::chrono::milliseconds(5000));             // max wait for the next message to start arriving
    acsf::setTimeout(acsf::READ_BODY_TIMEOUT_MS, std::chrono::milliseconds(5000));        // max wait for the rest of a message once it started
    acsf::setTimeout(acsf::SEND_TIMEOUT_MS, std::chrono::milliseconds(5000));             // max time to finish sending one message
    acsf::setTimeout(acsf::SERVER_HEARTBEAT_TIMEOUT_MS, std::chrono::milliseconds(5000)); // drop a connection that has been silent this long
    acsf::setTimeout(acsf::HEARTBEAT_RATE, std::chrono::milliseconds(2000));              // how often to send a heartbeat

    // ---- 2. Start the server ----
    // Logging is off by defualt.
    acsf::LoggerConfig serverLog;
    serverLog.enabled = true;
    serverLog.folder = "LOG_FOLDER/server";

    // Arguments: port, callback for when a client disconnects, "drop silent clients", "send heartbeats", logging config
    acsf::Server server(port,
                        [] { std::cout << "[server] a client disconnected\n"; },
                        /*keepAliveByHeartBeat=*/true, //Default value is true
                        /*sendHeartBeats=*/false, //Default value is false
                        serverLog);
    server.start();

    // receive() blocks until a message arrives (or returns nothing once the server stops), so give it its own thread.
    // Here we just send every message straight back to whoever sent it.
    std::thread serverThread([&server] {
        while (auto msg = server.receive()) {
            std::cout << "[server] got from client " << msg->isFrom << ": " << msg->payload << "\n";
            server.send(msg->isFrom, "echo: " + msg->payload);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // give the server a moment to start listening

    // ---- 3. Connect a client ----
    acsf::LoggerConfig clientLog;
    clientLog.enabled = true;
    clientLog.folder = "LOG_FOLDER/client";

    // Arguments: ip, port, callback for when the connection closes, "drop a silent server", "send heartbeats", logging config
    acsf::Client client("127.0.0.1", port,
                        [] { std::cout << "[client] connection closed\n"; },
                        /*keepAliveByHeartBeat=*/false,  //Default value is false
                        /*sendHeartBeats=*/true,  //Default value is true
                        clientLog);

    if (auto ec = client.start()) {            // start() returns an error code if it could not connect
        std::cerr << "[client] could not connect: " << ec.message() << "\n";
        server.stop();
        serverThread.join();
        return 1;
    }

    // ---- 4. Send and receive ----
    for (int i = 0; i < 3; ++i) {
        client.send("hello " + std::to_string(i));        // send() returns right away; the message is sent in the background
    }

    for (int i = 0; i < 3; ++i) {
        auto reply = client.receive();                    // waits until a message arrives
        if (!reply) break;                                // nothing returned means the connection closed
        std::cout << "[client] got: " << reply->payload << "\n";
    }

    // Stay connected a little: with heartbeats on, an idle connection is kept alive instead of being dropped
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // ---- 5. Shut down ----
    client.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // let the server notice the client leaving
    server.stop();
    serverThread.join();

    std::cout << "done (see LOG_FOLDER for the log files)\n";
    return 0;
}
