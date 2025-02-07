#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include "server.h"
#include "client.h"

using boost::asio::ip::tcp;

using std::cout;                     
using std::cin;
using std::cerr;

int main() {
    std::string userChoice;
    cout << "Are you a server or a client (s/c): ";
    cin >> userChoice;

    if(userChoice == "s"){
        // Create a io_context
        boost::asio::io_context io_context;

        // Create server object
        unsigned short port = 15555;
        Server server(io_context, port);

        // Start server & io_context
        server.start();

        // Start accepting connections
        server.asyncAcceptConnection();

        // Enable msg logging
        server.setlogDebug_(true);

        // Wait for client
        server.waitForConnections(1);
        
        // Send msg to client
        server.asyncSendMessageToAll("Hello from the server this is msg 1/6");
        server.asyncSendMessageToAll("Hello from the server 2/6");
        server.asyncSendMessageToAll("Hello 3/6");

        // Recv msg from client
        server.asyncReceiveMessage();

        server.asyncSendMessageToAll("Hi! 4/6");
        server.asyncSendMessageToAll("Test1 5/6");
        server.asyncSendMessageToAll("Test2 6/6");

        // Shut off
        //server.shutdown(true);

        io_context.run();

    }
    else{
        boost::asio::io_context io_context;

        unsigned short port = 15555;
        std::string serverIp = "127.0.0.1";

        Client client(io_context, serverIp, port);

        client.start();
        client.setlogDebug_(true);
        client.connect();
        client.asyncReceiveMessage();
        client.asyncSendMessage("Hello from client!");
        client.asyncSendMessage("Hello from client again!");
        client.asyncSendMessage("Hello serrver!");

        //client.shutdown(true);

        io_context.run();
    }

    return 0;
}

// TODO:
// Comment all code
// Add logger var to all scripts that controls how much we log
// Add some kind of callback to all funcs so that users can get the data recv'ed
// Add way to stop and start the task queue, use the already made stop_ bool