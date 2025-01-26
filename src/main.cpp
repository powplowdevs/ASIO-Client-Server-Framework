#include <boost/asio.hpp>
#include <iostream>
#include <string>

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
        // Create a io_context so we can use all the boost asio funcs
        boost::asio::io_context io_context;

        // Create server object
        unsigned short port = 55555;
        Server server(io_context, port);

        // Start server & io_context
        server.start();
        io_context.run();
    }
    else{
        boost::asio::io_context io_context;

        unsigned short port = 55555;
        std::string serverIp = "127.0.0.1";

        Client client(io_context, serverIp, port);

        client.start();
        io_context.run();
    }

    return 0;
}
