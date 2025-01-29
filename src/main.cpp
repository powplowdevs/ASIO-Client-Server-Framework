#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>


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

        // Wait for client
        server.waitForConnections(1);

        // Send msg to client
        server.asyncSendMessageToAll("EEEE WOAH WOAH WAOH!", "\n");

        io_context.run();

    }
    else{
        boost::asio::io_context io_context;

        unsigned short port = 15555;
        std::string serverIp = "127.0.0.1";

        Client client(io_context, serverIp, port);

        client.start();
        client.connect();
        client.receiveMessage("\n");

        io_context.run();
    }

    return 0;
}