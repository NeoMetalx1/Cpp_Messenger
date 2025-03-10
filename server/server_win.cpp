#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <cstring>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

class MessengerServer {
private:
    SOCKET server_socket;
    int port;
    std::map<SOCKET, std::string> clients; // socket -> username
    std::mutex clients_mutex;
    bool running;
    std::vector<std::thread> client_threads;

public:
    MessengerServer(int port) : port(port), running(false), server_socket(INVALID_SOCKET) {}

    ~MessengerServer() {
        stop();
    }

    bool start() {
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }

        // Create socket
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }

        // Setup server address
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        // Set address reuse option
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
            std::cerr << "Socket setup error: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        // Bind socket to address
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind socket: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        // Set socket to listen mode
        if (listen(server_socket, 10) == SOCKET_ERROR) {
            std::cerr << "Listen error: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        std::cout << "Server started on port " << port << std::endl;
        running = true;

        // Start the main connection acceptance loop
        accept_connections();
        return true;
    }

    void stop() {
        running = false;

        // Close server socket
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }

        // Close all client connections
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& client : clients) {
            closesocket(client.first);
        }
        clients.clear();

        // Wait for all threads to finish
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // Cleanup Winsock
        WSACleanup();

        std::cout << "Server stopped" << std::endl;
    }

private:
    void accept_connections() {
        while (running) {
            struct sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);

            SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_socket == INVALID_SOCKET) {
                if (running) {
                    std::cerr << "Error accepting connection: " << WSAGetLastError() << std::endl;
                }
                continue;
            }

            // Get client IP
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

            // Create new thread for client handling
            client_threads.push_back(std::thread(&MessengerServer::handle_client, this, client_socket));
        }
    }

    void handle_client(SOCKET client_socket) {
        char buffer[1024];

        // Get username
        int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            closesocket(client_socket);
            return;
        }

        buffer[bytes_read] = '\0';
        std::string username(buffer);

        // Add client to the list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[client_socket] = username;
        }

        // Send connection notification
        std::string connect_msg = username + " has joined the chat";
        broadcast_message(connect_msg, client_socket);

        // Main message processing loop
        while (running) {
            bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read <= 0) {
                break;
            }

            buffer[bytes_read] = '\0';
            std::string message(buffer);

            // Format and send message to all clients
            std::string formatted_msg = username + ": " + message;
            broadcast_message(formatted_msg, client_socket);
        }

        // Handle client disconnection
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            std::string disconnect_msg = username + " has left the chat";
            clients.erase(client_socket);
            broadcast_message(disconnect_msg, INVALID_SOCKET);
        }

        closesocket(client_socket);
    }

    void broadcast_message(const std::string& message, SOCKET sender_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);

        std::cout << message << std::endl;

        for (const auto& client : clients) {
            // Don't send message back to sender
            if (client.first != sender_socket) {
                send(client.first, message.c_str(), (int)message.length(), 0);
            }
        }
    }
};

int main() {
    int port = 8888;
    MessengerServer server(port);

    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    // Wait for Enter key to stop
    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();

    server.stop();
    return 0;
}
