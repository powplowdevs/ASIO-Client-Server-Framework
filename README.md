# Boost ASIO Client-Server Framework

This framework is a baseline setup for asynchronous communication between a server and multiple clients using Boost ASIO. It provides the essential tools to:

- **Establish Connections:** Create and manage a server that listens for connections, and clients that connect to it.
- **Send and Receive Messages:** Asynchronously send and receive messages between the server and clients.
- **Log Activity:** Record events and errors for troubleshooting.
- **Manage Asynchronous Tasks:** Use task and message queues to handle operations in the background.

---

## Project Status & Documentation

- ⚙️ Status: Completed (with potential future updates)
- 📚 **Documentation:** Detailed documentation is available at `/docs/html/index.html`
- 📁 **Examples:** Example usage can be found in `/src/examples`

---

## Getting Started

Before you begin, ensure you have the following installed on your machine:

- **Boost C++ Libraries**: The framework uses Boost ASIO, so make sure you have the Boost libraries set up. [Boost Installation Guide](https://www.boost.org/doc/libs/1_87_0/more/getting_started/index.html)
- **C++ Compiler**: Make sure you have a working C++ compiler installed (e.g., GCC, Clang, MSVC).

### Including Header Files

To use the framework, simply include all the header files in the `/include` directory. Since this is a header-file-only project, there is no need to compile any source files. All necessary functionality is contained in the headers. You may copy the headers into your own `includes` folder, configure your compiler to include the headers, or link them in your compile command.
