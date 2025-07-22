# Redis-like In-Memory Database (C++)

A high-performance, in-memory key-value database inspired by Redis, implemented in modern C++. This project features a custom binary protocol, efficient data structures, and basic multi-threading for background cleanup.

---

## Features

- **In-Memory Storage:** All data is stored in RAM for ultra-fast access (no persistence to disk).
- **Key-Value Store:** Supports basic commands: `SET`, `GET`, `DEL`, `KEYS`.
- **Sorted Sets:** Redis-like sorted set operations: `ZADD`, `ZSCORE`, `ZREM`, `ZQUERY`.
- **Expiration:** Keys can be set to expire automatically.
- **Custom Protocol:** Efficient binary protocol for client-server communication over TCP.
- **Multi-threaded Cleanup:** Uses a thread pool to safely delete complex data structures in the background.
- **Event-driven Server:** Handles multiple clients using non-blocking I/O and `poll`.

---

## Getting Started

### Prerequisites

- C++17 compatible compiler (e.g., `g++`)
- Unix-like OS (Linux, macOS)

### Build

```sh
make
```

This will build both the server and client binaries.

---

## Usage

### Start the Server

```sh
./server
```

The server listens on `localhost:1234`.

### Run a Client Command

```sh
./client set mykey myvalue
./client get mykey
./client del mykey
./client keys
./client zadd myzset 42.0 alice
./client zscore myzset alice
./client zrem myzset alice
```

---

## Architecture

- **Server:** Handles TCP connections, parses commands, and operates on in-memory data structures.
- **Client:** Sends commands to the server using the custom protocol.
- **Data Structures:** Custom hash tables, AVL trees, doubly linked lists, heaps, and thread pools.
- **Thread Pool:** Used for background deletion of sorted sets to avoid blocking the main server loop.

---

## Limitations

- **No Persistence:** All data is lost when the server stops.
- **Single-threaded Command Processing:** Only deletion of sorted sets is multi-threaded.
- **No Authentication or Security:** Intended for educational/demo purposes.

---

## Project Structure

- `server` / `Server.cpp` — Main server binary and logic
- `client` / `client.cpp` — Command-line client
- `hashtable.*`, `zset.*`, `AVL.*`, `DList.*`, `heap.*` — Core data structures
- `thread.*` — Thread pool implementation
- `serialisation.*` — Binary protocol serialization
- `test/` — Test code

---

## License

MIT License (or specify your own)

---

## Credits

Inspired by Redis and its data structures.
