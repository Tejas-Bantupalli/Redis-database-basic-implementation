# Redis-database-basic-implementation
This repository contains the source code for a basic Redis-like key-value store server and client written entirely in C++. The server listens for incoming connections and supports basic commands to manipulate the key-value store. The client can send requests to the server to perform operations like setting, getting, and deleting keys.

Features

Server:

1.Handles multiple client connections using polling.
2.Supports commands: set, get, del, keys.
3.Manages key-value pairs in a global map.
4.Provides a basic hashtable with dynamic resizing.
5.Gracefully shuts down on receiving termination signals.

Client:

1.Connects to the server and sends commands.
2.Receives and prints responses from the server.

Commands:

1.set <key> <value>: Sets the value for the specified key.
2.get <key>: Retrieves the value for the specified key.
3.del <key>: Deletes the specified key.
4.keys: Lists all keys in the store.
