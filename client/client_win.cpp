#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // Подключение библиотеки WinSock

class MessengerClient {
private:
    SOCKET client_socket;
    std::string server_ip;
    int server_port;
    std::string username;
    std::atomic<bool> running;
    std::thread receive_thread;

public:
    MessengerClient(const std::string& server_ip, int server_port, const std::string& username)
        : server_ip(server_ip), server_port(server_port), username(username), running(false), client_socket(INVALID_SOCKET) {
    }

    ~MessengerClient() {
        disconnect();
    }

    bool connect() {
        // Инициализация WinSock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize WinSock" << std::endl;
            return false;
        }

        // Создание сокета
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }

        // Настройка адреса сервера
        struct sockaddr_in server_addr;
        ZeroMemory(&server_addr, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);

        // Преобразование IP-адреса из строкового формата в бинарный
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address or address not supported" << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return false;
        }

        // Подключение к серверу
        if (::connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return false;
        }

        std::cout << "Connected to server at " << server_ip << ":" << server_port << std::endl;

        // Отправка имени пользователя на сервер
        if (send(client_socket, username.c_str(), username.length(), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send username: " << WSAGetLastError() << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return false;
        }

        running = true;

        // Запуск потока для приема сообщений
        receive_thread = std::thread(&MessengerClient::receive_messages, this);

        return true;
    }

    void disconnect() {
        running = false;

        // Закрытие сокета
        if (client_socket != INVALID_SOCKET) {
            shutdown(client_socket, SD_BOTH);
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }

        // Ожидание завершения потока приема
        if (receive_thread.joinable()) {
            receive_thread.join();
        }

        // Очистка WinSock
        WSACleanup();

        std::cout << "Disconnected from server" << std::endl;
    }

    bool send_message(const std::string& message) {
        if (!running || client_socket == INVALID_SOCKET) {
            std::cerr << "Not connected to server" << std::endl;
            return false;
        }

        // Отправка сообщения с надлежащей обработкой ошибок
        int bytes_sent = send(client_socket, message.c_str(), message.length(), 0);
        if (bytes_sent == SOCKET_ERROR) {
            std::cerr << "Failed to send message: " << WSAGetLastError() << std::endl;
            return false;
        }
        else if (bytes_sent == 0) {
            std::cerr << "Connection closed by server" << std::endl;
            return false;
        }
        else if (static_cast<size_t>(bytes_sent) < message.length()) {
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

        while (running && client_socket != INVALID_SOCKET) {
            ZeroMemory(buffer, sizeof(buffer));
            int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

            if (bytes_read == SOCKET_ERROR) {
                if (WSAGetLastError() != WSAEINTR && running) {
                    std::cerr << "Error receiving data: " << WSAGetLastError() << std::endl;
                    running = false;
                }
                break;
            }
            else if (bytes_read == 0) {
                if (running) {
                    std::cout << "Server closed the connection" << std::endl;
                    running = false;
                }
                break;
            }
            else {
                buffer[bytes_read] = '\0';
                std::cout << buffer << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1"; // По умолчанию localhost
    int server_port = 8888;              // Порт по умолчанию
    std::string username;

    // Обработка аргументов командной строки
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = std::stoi(argv[2]);
    }

    // Получение имени пользователя
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    if (username.empty()) {
        std::cerr << "Username cannot be empty" << std::endl;
        return 1;
    }

    // Создание и подключение клиента
    MessengerClient client(server_ip, server_port, username);

    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Запуск сессии чата
    client.start_chat();

    return 0;
}
