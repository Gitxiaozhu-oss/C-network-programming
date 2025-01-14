#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h> // 包含 TCP_NODELAY 的头文件

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024 * 1024 // 1MB per chunk

void enable_tcp_options(int sockfd) {
    int optval = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) { // 启用 TCP_NODELAY 选项
        perror("Failed to set TCP_NODELAY");
        exit(EXIT_FAILURE);
    }
}

bool receive_file_chunk(int sockfd, char* buffer, size_t& length) {
    ssize_t bytes_received = recv(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL); // 接收数据
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cout << "Connection closed by server" << std::endl;
        } else {
            perror("Failed to receive data");
        }
        return false;
    }
    length = static_cast<size_t>(bytes_received);
    return true;
}

void client_thread(const std::string& output_file_path) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // 创建套接字
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr)<=0) { // 将 IP 地址转换为网络格式
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // 连接到服务器
        printf("\nConnection Failed \n");
        return;
    }

    enable_tcp_options(sock); // 启用 TCP 选项

    std::ofstream outfile(output_file_path, std::ios::binary | std::ios::app);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open output file: " << output_file_path << std::endl;
        close(sock);
        return;
    }

    char buffer[BUFFER_SIZE];

    while (true) {
        size_t length = BUFFER_SIZE;

        if (!receive_file_chunk(sock, buffer, length)) { // 接收文件块
            break;
        }

        if (length == 0) {
            std::cout << "End of file received" << std::endl;
            break;
        }

        outfile.write(buffer, length); // 写入文件
        std::cout << "Received " << length << " bytes" << std::endl;
    }

    outfile.close();
    shutdown(sock, SHUT_WR);
    close(sock);
    std::cout << "File transfer completed" << std::endl;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output_file_path>" << std::endl;
        return -1;
    }

    std::string output_file_path = argv[1];
    std::cout << "Output file path: " << output_file_path << std::endl; // Debugging line

    // Check if the file path is valid and writable
    std::ofstream test_output(output_file_path, std::ios::binary | std::ios::app);
    if (!test_output.is_open()) {
        std::cerr << "The specified output file path is invalid or not writable." << std::endl;
        return -1;
    }
    test_output.close();

    client_thread(output_file_path); // 启动客户端线程

    return 0;
}




