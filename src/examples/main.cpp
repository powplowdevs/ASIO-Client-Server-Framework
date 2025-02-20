/**
 * @file main.cpp
 * @brief Example usage of the Boost ASIO Client-Server Framework.
 * 
 * This example demonstrates how to use the Boost ASIO Client-Server Framework for creating a simple 
 * client-server application. It allows the user to choose to either run as a server or client. 
 * The server listens for client connections and exchanges messages with the client asynchronously.
 * 
 * 
 * @author Ayoub Mohamed (powplowdevs on GitHub)
 * @license MIT License. See LICENSE file for details.
 * 
 * @example main.cpp
 * This example demonstrates how to use the Boost ASIO Client-Server Framework for creating a simple client-server application.
 * The code below shows how the server and client are implemented:
 * 
 * Below is the main function to run the client-server example.
 * 
 * The user is prompted to choose whether they want to run as a server or a client.
 * - If the user selects "s" for server, it sets up a server to listen for connections,
 *   send a message to clients, and receive a response asynchronously.
 * - If the user selects "c" for client, it connects to the server, sends a message, 
 *   and waits for a response.
 * 
 * @note To compile the example, use g++ (or similar C++ compiler) with the following command:
 *       g++ main.cpp -o main.exe -I../../include -pthread -std=c++17
 *       After compiling, you can run the program using:
 *       ./main.exe
 * 
 * @code
 */

 #include <boost/asio.hpp>   // Include Boost ASIO for asynchronous networking
 #include <iostream>
 #include <string>
 #include <thread>
 #include <atomic>
 #include <chrono>
 
 #include "server.h"          // Include custom header for server functionality
 #include "client.h"          // Include custom header for client functionality
 
 using boost::asio::ip::tcp;   // Use TCP protocol for client-server communication
 using std::cout;              // Use standard output
 using std::cin;               // Use standard input
 using std::cerr;              // Use standard error output
 
 int main() {
     std::string userChoice;  // Variable to store user input (server or client)
     cout << "Are you a server or a client (s/c): ";
     cin >> userChoice;  // Get user input to decide if running as server or client
 
     if(userChoice == "s"){
         // Server side code
         // @example server
         // Create an io_context, necessary for Boost ASIO asynchronous operations
         boost::asio::io_context io_context;
 
         // Create a server object, set port to 15555
         unsigned short port = 15555;
         Server server(io_context, port);
 
         // Start server & io_context (io_context handles asynchronous tasks)
         server.start();
 
         // Start accepting incoming client connections asynchronously
         server.asyncAcceptConnection();
 
         // Enable message logging for debugging
         server.setlogDebug_(true);
 
         // Wait for client connections (only 1 client in this example)
         server.waitForConnections(1);
 
         // Send a message to all connected clients (in this case, just one)
         server.asyncSendMessageToAll("Hello from the server!");
 
         // Receive a message asynchronously from the client
         server.asyncReceiveMessage();
 
         // Get the last received message from the server and display it
         std::string msg = std::any_cast<std::string>(server.getLastMessage());
         cout << "MSG FROM SERVER: " << msg << std::endl;
 
         // Run the io_context to process the asynchronous operations
         io_context.run();
     }
     else{
         // Client side code
         // @example client
         // Create an io_context, necessary for Boost ASIO asynchronous operations
         boost::asio::io_context io_context;
 
         // Set server IP and port
         unsigned short port = 15555;
         std::string serverIp = "127.0.0.1";  // Localhost for testing
 
         // Create a client object
         Client client(io_context, serverIp, port);
 
         // Start the client & io_context (io_context handles asynchronous tasks)
         client.start();
 
         // Enable message logging for debugging
         client.setlogDebug_(true);
 
         // Establish connection to the server
         client.connect();
 
         // Send a message to the server asynchronously
         client.asyncSendMessage("Hello from the client!");
 
         // Receive a message asynchronously from the server
         client.asyncReceiveMessage();
 
         // Get the last received message from the client and display it
         std::string msg = std::any_cast<std::string>(client.getLastMessage());
         cout << "MSG FROM SERVER: " << msg << std::endl;
 
         // Run the io_context to process the asynchronous operations
         io_context.run();
     }
 
     return 0;
 }
 
 /**
  * @endcode
  */
 