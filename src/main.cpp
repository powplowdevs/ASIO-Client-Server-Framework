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
        server.asyncSendMessageToAll("Hello from the server!");

        // Recv msg from client
        server.asyncReceiveMessage();
        std::string msg = std::any_cast<std::string>(server.getLastMessage());
        cout << "MSG FROM SERVER: " << msg << std::endl;


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

        client.asyncSendMessage("Hello from the client!");

        client.asyncReceiveMessage();
        std::string msg = std::any_cast<std::string>(client.getLastMessage());
        cout << "MSG FROM SERVER: " << msg << std::endl;

        //client.shutdown(true);

        io_context.run();
    }

    return 0;
}

// TODO:
// Comment all code
// Abstract client and server duped code to shared class