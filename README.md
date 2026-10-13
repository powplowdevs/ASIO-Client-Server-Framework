# ASIO Client-Server Framework

A header-only C++ networking library built on Boost.Asio for asynchronous TCP messaging between one server and multiple clients, using async accept, read and write calls.

## Usage

A full example with timeouts, heartbeats and logging is in [src/examples/main.cpp](src/examples/main.cpp).

Copy the `include/` folder into your project (or add it to your include path) and include `server.h` and/or `client.h`.

```cpp
#include "server.h"
#include "client.h"

// Server: echo every message back to its sender
acsf::Server server(15555, [] { /* a client disconnected */ });
server.start();

while (auto msg = server.receive()) {            // blocks until a message arrives
    server.send(msg->isFrom, "echo: " + msg->payload);
}

// Client
acsf::Client client("127.0.0.1", 15555);
if (auto ec = client.start()) { /* could not connect */ }

client.send("hello");
auto reply = client.receive();                   // blocks until a message arrives
```

## Documentation
Doxygen docs can be found [here](https://powplowdevs.github.io/docs/html/index.html)

## Project layout

```
include/                 the library (headers only)
    server.h             Server: accepts clients, send/receive
    client.h             Client: connects to a server, send/receive
    connection.h         one TCP connection: read/write loops, heartbeats, timeouts
    connection_map.h     the server's set of active connections
    message_queue.h      incoming messages waiting for receive()
    outbox_queue.h       outgoing messages waiting to be written
    timeout_manager.h    timers used for timeouts and heartbeats
    protocol.h           message framing and default settings
    logger.h             file logger and LoggerConfig
src/examples/main.cpp    basic server + client example
tests/simple_bench.cpp   throughput and latency benchmark
```

## Requirements

- A C++23 compiler (tested with `g++-13`)
- Boost (Asio)

<hr>

## Benchmark

`tests/simple_bench.cpp` runs one server and one client. The server echoes every message back. The client keeps N messages in flight, records how long each echo takes, and reports throughput and latency for several values of N.

### Results

Median of 3 runs. 64-byte messages, client and server on the same machine over loopback.

| Messages in flight | Messages/sec | p50 latency | p99 latency |
|---|---|---|---|
| 1 | 37k | 22 µs | 55 µs |
| 4 | 153k | 25 µs | 37 µs |
| 16 | 160k | 98 µs | 134 µs |
| 64 | 151k | 420 µs | 511 µs |
| 256 | 154k | 1.7 ms | 1.9 ms |
| 1024 | 154k | 6.7 ms | 7.4 ms |

With 4 messages in flight the server echoes about 153k messages per second with a p99 latency of 37 µs. Throughput tops out around 160k messages per second. Past that, more messages in flight only add queueing delay. A message is counted when its echo comes back, so one "message" here is a full round trip (client to server and back).

### Settings used

| Setting | Value |
|---|---|
| Heartbeats (server and client, sending and requiring) | **off** |
| Logging | **off** |
| Message size | 64 bytes |
| Per run | 2 s warmup (not counted) + 5 s measured |
| Timeouts | library defaults (5 s read / body / send, 2 s heartbeat rate) |

Heartbeats had no measurable effect: with every heartbeat setting turned on, 4 messages in flight gave 151k messages per second and a p99 of 36 µs. Logging was not benchmarked. Furthermore the client and server run in the same process and share the machine, and loopback has no real network in it numbers will differ on other hardware, especially with a real network between the two sides.

### Where it was run

A consumer laptop

- Intel Core Ultra 7 256V (8 cores), 16 GB RAM
- Ubuntu Linux, kernel 6.17, g++ 13.4, Boost 1.74
