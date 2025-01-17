#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <random>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring> // 包含 memset 的头文件

// 生成随机字符串
std::string generate_random_string(std::size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::string random_str(length, ' ');
    std::mt19937 generator(static_cast<unsigned long>(time(nullptr)));
    std::uniform_int_distribution<> distribution(0, sizeof(alphanum) - 2);

    for (std::size_t i = 0; i < length; ++i) {
        random_str[i] = alphanum[distribution(generator)];
    }

    return random_str;
}

// 客户端线程函数
void client_thread(int id, std::size_t num_messages, std::size_t message_size) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        return;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        perror("connect");
        close(client_socket);
        return;
    }

    std::cout << "Client " << id << " connected to server." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < num_messages; ++i) {
        std::string message = generate_random_string(message_size);
        send(client_socket, message.c_str(), message.size(), 0);

        char buffer[1024];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break; // 连接关闭或错误
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Client " << id << " sent " << num_messages << " messages in " << duration << " ms." << std::endl;

    close(client_socket);
}

int main() {
    const int num_clients = 10; // 客户端数量
    const std::size_t num_messages_per_client = 100; // 每个客户端发送的消息数量
    const std::size_t message_size = 128; // 每条消息的大小

    std::vector<std::thread> clients;

    for (int i = 0; i < num_clients; ++i) {
        clients.emplace_back(client_thread, i + 1, num_messages_per_client, message_size);
    }

    for (auto& client : clients) {
        client.join();
    }

    return 0;
}




