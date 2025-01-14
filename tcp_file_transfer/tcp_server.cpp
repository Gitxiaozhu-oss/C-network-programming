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
#include <sys/stat.h>
#include <netinet/tcp.h> // 包含 TCP_NODELAY 的头文件

#define PORT 8080
#define BUFFER_SIZE 1024 * 1024 // 1MB per chunk

void enable_tcp_options(int sockfd) {
    int optval = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) { // 启用 TCP_NODELAY 选项
        perror("Failed to set TCP_NODELAY");
        exit(EXIT_FAILURE);
    }
}

void send_file_chunk(int sockfd, int file_fd, off_t offset, size_t length) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = pread(file_fd, buffer, length, offset); // 从文件读取数据
    if (bytes_read <= 0) {
        std::cerr << "Failed to read file at offset " << offset << ", bytes_read: " << bytes_read << std::endl;
        return;
    }

    ssize_t bytes_sent = send(sockfd, buffer, bytes_read, 0); // 发送数据
    if (bytes_sent != bytes_read) {
        std::cerr << "Failed to send all data. Sent " << bytes_sent << " out of " << bytes_read << " bytes." << std::endl;
    }
}

void server_thread(int client_sockfd, const std::string& file_path) {
    int file_fd = open(file_path.c_str(), O_RDONLY); // 打开文件
    if (file_fd < 0) {
        perror("Failed to open file");
        close(client_sockfd);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) { // 获取文件状态
        perror("Failed to get file stats");
        close(file_fd);
        close(client_sockfd);
        return;
    }
    size_t file_size = file_stat.st_size;

    for (off_t offset = 0; offset < file_size; offset += BUFFER_SIZE) {
        size_t length = static_cast<size_t>(std::min(static_cast<long long>(file_size - offset), static_cast<long long>(BUFFER_SIZE))); // 计算当前块的长度
        send_file_chunk(client_sockfd, file_fd, offset, length); // 发送文件块
        std::cout << "Sent " << length << " bytes from offset " << offset << std::endl;
    }

    // Send a zero-length chunk to indicate end of file
    char empty_buffer[1] = {0};
    send(client_sockfd, empty_buffer, 0, 0);
    std::cout << "Sent end-of-file indicator" << std::endl;

    close(file_fd);
    close(client_sockfd);
    std::cout << "Client connection closed" << std::endl;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file_path>" << std::endl;
        return -1;
    }

    std::string file_path = argv[1];

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // 创建套接字
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { // 设置套接字选项
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // 绑定地址
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) { // 监听连接
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { // 接受连接
            perror("accept");
            continue;
        }

        enable_tcp_options(new_socket); // 启用 TCP 选项

        std::thread t(server_thread, new_socket, file_path); // 创建线程处理客户端连接
        t.detach();
        std::cout << "New client connected and thread started" << std::endl;
    }

    close(server_fd);
    return 0;
}




