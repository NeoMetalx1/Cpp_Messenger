#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

class MessengerClient {
private:
    int client_socket;
    std::string server_ip;
    int server_port;
    std::string username;
    std::atomic<bool> running;
    std::thread receive_thread;

public:
    MessengerClient(const std::string& server_ip, int server_port, const std::string& username)
        : server_ip(server_ip), server_port(server_port), username(username), running(false), client_socket(-1) {}

    ~MessengerClient() {
        disconnect();
    }

    bool connect() {
        // Create socket
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Setup server address
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);

        // Convert IP address from string to binary form
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address or address not supported" << std::endl;
            close(client_socket);
            return false;
        }

        // Connect to server
        if (::connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Connection failed" << std::endl;
            close(client_socket);
            return false;
        }

        std::cout << "Connected to server at " << server_ip << ":" << server_port << std::endl;

        // Send username to server
        if (send(client_socket, username.c_str(), username.length(), 0) < 0) {
            std::cerr << "Failed to send username" << std::endl;
            close(client_socket);
            return false;
        }

        running = true;

        // Start thread for receiving messages
        receive_thread = std::thread(&MessengerClient::receive_messages, this);

        return true;
    }

    void disconnect() {
        running = false;

        // Close socket
        if (client_socket != -1) {
            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
            client_socket = -1;
        }

        // Wait for receive thread to finish
        if (receive_thread.joinable()) {
            receive_thread.join();
        }

        std::cout << "Disconnected from server" << std::endl;
    }

    bool send_message(const std::string& message) {
        if (!running || client_socket == -1) {
            std::cerr << "Not connected to server" << std::endl;
            return false;
        }

        // Send the message with proper error handling
        ssize_t bytes_sent = send(client_socket, message.c_str(), message.length(), 0);
        if (bytes_sent < 0) {
            std::cerr << "Failed to send message: " << strerror(errno) << std::endl;
            return false;
        } else if (bytes_sent == 0) {
            std::cerr << "Connection closed by server" << std::endl;
            return false;
        } else if (static_cast<size_t>(bytes_sent) < message.length()) {
            std::cerr << "Warning: Only sent " << bytes_sent << " of " << message.length() << " bytes" << std::endl;
        }

        return true;
    }

    void start_chat() {
        std::string message;
        std::cout << "Start typing messages (type 'exit' to quit):" << std::endl;

        while (running) {
            std::getline(std::cin, message);

            if (message == "exit") {
                break;
            }

            if (!message.empty()) {
                std::cout << "You: " << message << std::endl;
                if (!send_message(message)) {
                    std::cerr << "Message sending failed, disconnecting..." << std::endl;
                    break;
                }
            }
        }

        disconnect();
    }

private:
    void receive_messages() {
        char buffer[1024];
        
        while (running && client_socket != -1) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read < 0) {
                if (errno != EINTR && running) {
                    std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
                    running = false;
                }
                break;
            } else if (bytes_read == 0) {
                if (running) {
                    std::cout << "Server closed the connection" << std::endl;
                    running = false;
                }
                break;
            } else {
                buffer[bytes_read] = '\0';
                std::cout << buffer << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1"; // Default to localhost
    int server_port = 8888;             // Default port
    std::string username;

    // Process command line arguments
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = std::stoi(argv[2]);
    }

    // Get username
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    if (username.empty()) {
        std::cerr << "Username cannot be empty" << std::endl;
        return 1;
    }

    // Create and connect client
    MessengerClient client(server_ip, server_port, username);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Start chat session
    client.start_chat();

    return 0;
}
