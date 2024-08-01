# Redis-database-basic-implementation
This repository contains the source code for a basic Redis-like key-value store server and client written entirely in C++. The server listens for incoming connections and supports basic commands to manipulate the key-value store. The client can send requests to the server to perform operations like setting, getting, and deleting keys.

Features
Server:

Handles multiple client connections using polling.
Supports commands: set, get, del, keys.
Manages key-value pairs in a global map.
Provides a basic hashtable with dynamic resizing.
Gracefully shuts down on receiving termination signals.
Client:

Connects to the server and sends commands.
Receives and prints responses from the server.
Commands
set <key> <value>: Sets the value for the specified key.
get <key>: Retrieves the value for the specified key.
del <key>: Deletes the specified key.
keys: Lists all keys in the store.
