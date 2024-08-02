#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <thread>

#define PORT 9090
#define BACKLOG 10
#define MAX_MSG_SIZE 4096

std::unordered_map<std::string, std::string> store;

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void handle_client(int client_fd) {
    char buffer[MAX_MSG_SIZE] = {0};
    ssize_t bytes_received = 0;

    while (bytes_received < sizeof(uint32_t)) {
        ssize_t result = recv(client_fd, buffer + bytes_received, sizeof(uint32_t) - bytes_received, 0);
        if (result < 0) {
            perror("recv");
            close(client_fd);
            return;
        }
        bytes_received += result;
    }

    uint32_t len;
    memcpy(&len, buffer, sizeof(uint32_t));
    len = ntohl(len);

    if (len > MAX_MSG_SIZE) {
        std::cerr << "Message too large: " << len << std::endl;
        close(client_fd);
        return;
    }

    while (bytes_received < len) {
        ssize_t result = recv(client_fd, buffer + bytes_received, len - bytes_received, 0);
        if (result < 0) {
            perror("recv");
            close(client_fd);
            return;
        }
        bytes_received += result;
    }

    std::string response(buffer, len);

    std::cout << "Received response:" << std::endl;
    std::cout << response << std::endl;

    close(client_fd);
}


void server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) die("socket");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        die("setsockopt");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(server_fd, BACKLOG) < 0)
        die("listen");

    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::thread(handle_client, client_fd).detach();
    }
}



void print_hex(const char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        std::cout << std::hex << std::uppercase 
                  << ((int)(unsigned char)data[i] < 16 ? "0" : "") 
                  << (int)(unsigned char)data[i] << " ";
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <IP> <port> <command> [<key> [<value>]]" << std::endl;
        return 1;
    }

    const char *ip = argv[1];
    int port = std::stoi(argv[2]);
    const char *command = argv[3];
    const char *key = (argc > 4) ? argv[4] : "";
    const char *value = (argc > 5) ? argv[5] : "";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return -1;
    }

    // Prepare the request
    std::vector<std::string> args;
    args.push_back(command);
    if (strcmp(command, "set") == 0) {
        if (argc != 6) {
            std::cerr << "Usage for set: " << argv[0] << " <IP> <port> set <key> <value>" << std::endl;
            return 1;
        }
        args.push_back(key);
        args.push_back(value);
    } else if (strcmp(command, "get") == 0 || strcmp(command, "del") == 0) {
        if (argc != 5) {
            std::cerr << "Usage for " << command << ": " << argv[0] << " <IP> <port> " << command << " <key>" << std::endl;
            return 1;
        }
        args.push_back(key);
    } else if (strcmp(command, "keys") == 0) {
    } else {
        std::cerr << "Invalid command" << std::endl;
        close(sock);
        return -1;
    }

    // Calculate total message size
    uint32_t total_size = 4; // 4 bytes for number of arguments
    for (const auto &arg : args) {
        total_size += 4 + arg.length(); // 4 bytes for length + size of the argument
    }

    // Prepare the request
    std::vector<uint8_t> request;
    request.reserve(4 + total_size); // 4 bytes for total size + total_size

    // Add total message size
    uint32_t net_total_size = htonl(total_size);
    request.insert(request.end(), (uint8_t*)&net_total_size, (uint8_t*)&net_total_size + 4);

    // Add number of arguments
    uint32_t net_num_args = htonl(args.size());
    request.insert(request.end(), (uint8_t*)&net_num_args, (uint8_t*)&net_num_args + 4);

    // Add arguments
    for (const auto &arg : args) {
        uint32_t net_arg_len = htonl(arg.length());
        request.insert(request.end(), (uint8_t*)&net_arg_len, (uint8_t*)&net_arg_len + 4);
        request.insert(request.end(), arg.begin(), arg.end());
    }

    // Send the request
    send(sock, request.data(), request.size(), 0);

    // Receive the response
    char response_buffer[MAX_MSG_SIZE];
    ssize_t bytes_received = recv(sock, response_buffer, MAX_MSG_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(sock);
        return -1;
    }
    response_buffer[bytes_received] = '\0';  // Null-terminate the received data

    // Print the response
    std::cout << response_buffer << std::endl;

    close(sock);
    return 0;
}
