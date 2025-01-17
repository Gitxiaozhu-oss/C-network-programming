#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

// 内存池类，用于管理固定大小的内存块
class MemoryPool {
public:
    // 构造函数，接受参数指定内存池的总大小和单个内存块的大小
    MemoryPool(std::size_t total_size, std::size_t block_size)
        : block_size_(block_size),
          num_blocks_(total_size / block_size),
          memory_(num_blocks_ * block_size),
          free_list_(num_blocks_) {
        // 初始化空闲列表，指向每个内存块的起始位置
        for (std::size_t i = 0; i < num_blocks_; ++i) {
            free_list_[i] = &memory_[i * block_size];
        }
    }

    // 分配一块内存，如果内存不足则返回nullptr
    void* allocate() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (free_list_.empty()) {
            return nullptr; // 没有可用的内存块
        }
        void* ptr = free_list_.back();
        free_list_.pop_back();
        return ptr;
    }

    // 释放一块内存，使其可以被再次分配
    void deallocate(void* ptr) {
        if (!ptr) {
            return; // 空指针不需要释放
        }
        std::unique_lock<std::mutex> lock(mutex_);
        free_list_.push_back(ptr);
    }

private:
    std::size_t block_size_; // 单个内存块的大小
    std::size_t num_blocks_; // 内存池中内存块的数量
    std::vector<char> memory_; // 存储内存池的所有字节
    std::vector<void*> free_list_; // 存储空闲内存块的指针
    std::mutex mutex_; // 互斥锁，保证线程安全
};

// 处理客户端连接的线程函数
void handle_client(int client_socket, sockaddr_in client_addr, MemoryPool& pool) {
    char buffer[1024];
    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break; // 连接关闭或错误
        }

        // 使用内存池分配内存块来存储接收到的数据
        void* data_block = pool.allocate();
        if (!data_block) {
            std::cerr << "Memory allocation failed!" << std::endl;
            break;
        }

        // 复制数据到分配的内存块
        memcpy(data_block, buffer, bytes_received);

        // 获取客户端的IP地址
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        // 打印接收到的消息及其相关信息
        std::cout << "Received message from " << client_ip << ": "
                  << "Length: " << bytes_received << ", Message: "
                  << std::string(static_cast<char*>(data_block), bytes_received) << std::endl;

        // 发送响应
        const char* response = "ACK";
        send(client_socket, response, strlen(response), 0);

        // 释放内存块
        pool.deallocate(data_block);
    }

    close(client_socket);
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_socket == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        close(server_socket);
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;

    MemoryPool pool(1024 * 1024, 1024); // 1MB内存池，每个块1KB

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_socket == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // 没有新的连接
            } else {
                perror("accept");
                break;
            }
        }

        std::thread client_thread(handle_client, client_socket, client_addr, std::ref(pool));
        client_thread.detach(); // 分离线程，让其独立运行
    }

    close(server_socket);
    return 0;
}




