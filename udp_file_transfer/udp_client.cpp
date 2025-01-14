#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> // 引入 inet_addr 的声明
#include <iomanip>      // 引入 setw 和 setfill
#include <cmath>        // 引入 ceil

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1472 // MTU size minus IP and UDP headers (1500 - 20 - 8 = 1472)

void send_file_with_progress(const std::string& filename, int client_socket, const struct sockaddr_in& server_addr) {
    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return;
    }

    // Get the size of the file
    infile.seekg(0, std::ios::end);
    size_t total_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    char buffer[BUFFER_SIZE];
    size_t sent_bytes = 0;

    while (infile.read(buffer, BUFFER_SIZE)) {
        size_t bytes_to_send = infile.gcount();
        sendto(client_socket, buffer, bytes_to_send, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        sent_bytes += bytes_to_send;

        // Update progress bar
        double progress = static_cast<double>(sent_bytes) / total_size;
        int bar_width = 50;
        std::cout << "[";
        int pos = bar_width * progress;
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << " %\r";
        std::cout.flush();
    }

    // Send remaining bytes
    size_t remaining_bytes = infile.gcount();
    if (remaining_bytes > 0) {
        sendto(client_socket, buffer, remaining_bytes, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        sent_bytes += remaining_bytes;

        // Update progress bar for remaining bytes
        double progress = static_cast<double>(sent_bytes) / total_size;
        int bar_width = 50;
        std::cout << "[";
        int pos = bar_width * progress;
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << " %\n";
        std::cout.flush();
    }

    infile.close();
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Creating socket file descriptor
    if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // Filling server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // 使用 inet_addr 转换 IP 地址

    std::string filename;
    std::cout << "Enter the file to send: ";
    std::cin >> filename;

    send_file_with_progress(filename, client_socket, server_addr);

    close(client_socket);
    return 0;
}




