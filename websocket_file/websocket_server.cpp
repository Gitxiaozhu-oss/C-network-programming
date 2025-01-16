#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define THREAD_POOL_SIZE 4

// 设置套接字为非阻塞模式
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

// 计算 Sec-WebSocket-Accept 值
std::string compute_accept_key(const std::string& key) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), hash);

    BIO* bio, * b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Avoid new line character at the end of encoded string
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);

    std::string ret(bufferPtr->data, bufferPtr->length);
    BUF_MEM_free(bufferPtr);

    return ret;
}

// 处理 HTTP 握手请求
bool handle_handshake(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        return false;
    }
    buffer[bytes_received] = '\0';

    std::string request(buffer);
    size_t upgrade_pos = request.find("Upgrade: websocket");
    size_t connection_pos = request.find("Connection: Upgrade");
    size_t key_pos = request.find("Sec-WebSocket-Key:");

    if (upgrade_pos == std::string::npos || connection_pos == std::string::npos || key_pos == std::string::npos) {
        return false;
    }

    key_pos += 19; // 移动到键值开始位置
    size_t key_end_pos = request.find("\r\n", key_pos);
    if (key_end_pos == std::string::npos) {
        return false;
    }

    std::string sec_websocket_key = request.substr(key_pos, key_end_pos - key_pos);
    std::string accept_key = compute_accept_key(sec_websocket_key);

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";

    send(client_socket, response.c_str(), response.size(), 0);
    return true;
}

// 解析 WebSocket 数据帧
std::pair<bool, std::string> parse_frame(char* data, ssize_t length) {
    bool fin = (data[0] & 0x80) != 0;
    uint8_t opcode = data[0] & 0x0F;
    bool mask = (data[1] & 0x80) != 0;
    size_t payload_len = data[1] & 0x7F;

    size_t header_len = 2;
    if (payload_len == 126) {
        payload_len = (static_cast<uint16_t>(data[2]) << 8) | static_cast<uint16_t>(data[3]);
        header_len = 4;
    } else if (payload_len == 127) {
        payload_len = ((static_cast<uint64_t>(data[2]) << 56) |
                       (static_cast<uint64_t>(data[3]) << 48) |
                       (static_cast<uint64_t>(data[4]) << 40) |
                       (static_cast<uint64_t>(data[5]) << 32) |
                       (static_cast<uint64_t>(data[6]) << 24) |
                       (static_cast<uint64_t>(data[7]) << 16) |
                       (static_cast<uint64_t>(data[8]) << 8) |
                       static_cast<uint64_t>(data[9]));
        header_len = 10;
    }

    if (mask) {
        uint8_t masking_key[4];
        memcpy(masking_key, data + header_len, 4);
        header_len += 4;
        for (size_t i = 0; i < payload_len; ++i) {
            data[header_len + i] ^= masking_key[i % 4];
        }
    }

    return {fin, std::string(data + header_len, payload_len)};
}

// 构建 WebSocket 数据帧
std::string build_frame(const std::string& message) {
    std::string frame;
    frame.push_back(0x81); // FIN bit set and text frame opcode
    size_t len = message.length();
    if (len <= 125) {
        frame.push_back(static_cast<char>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        frame.push_back((len >> 56) & 0xFF);
        frame.push_back((len >> 48) & 0xFF);
        frame.push_back((len >> 40) & 0xFF);
        frame.push_back((len >> 32) & 0xFF);
        frame.push_back((len >> 24) & 0xFF);
        frame.push_back((len >> 16) & 0xFF);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    }
    frame.append(message);
    return frame;
}

// 工作线程函数
void worker_thread(int epoll_fd, std::queue<int>& work_queue, std::mutex& queue_mutex,
                   std::condition_variable& cv, std::unordered_map<int, sockaddr_in>& clients) {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [&work_queue] { return !work_queue.empty(); });
        int client_socket = work_queue.front();
        work_queue.pop();
        lock.unlock();

        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            close(client_socket);
            std::lock_guard<std::mutex> guard(queue_mutex);
            clients.erase(client_socket);
            continue;
        }

        auto [fin, message] = parse_frame(buffer, bytes_received);
        if (!fin) {
            continue; // 不支持分片消息
        }

        std::cout << "Received message from client " << client_socket << ": " << message << std::endl;

        // 广播消息给所有客户端
        std::string frame = build_frame("Broadcast: " + message);
        for (const auto& [fd, _] : clients) {
            send(fd, frame.c_str(), frame.size(), MSG_NOSIGNAL);
        }
    }
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

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_socket);
        return 1;
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
        perror("epoll_ctl: listen_sock");
        close(server_socket);
        close(epoll_fd);
        return 1;
    }

    std::queue<int> work_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::unordered_map<int, sockaddr_in> clients;

    std::vector<std::thread> workers;
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        workers.emplace_back(worker_thread, epoll_fd, std::ref(work_queue), std::ref(queue_mutex),
                             std::ref(cv), std::ref(clients));
    }

    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == server_socket) {
                while (true) {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_socket = accept4(server_socket, reinterpret_cast<sockaddr*>(&client_addr),
                                                &client_len, SOCK_NONBLOCK);
                    if (client_socket == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 没有新的连接
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    std::cout << "New client connected: " 
                              << inet_ntoa(client_addr.sin_addr) << ":" 
                              << ntohs(client_addr.sin_port) << std::endl;

                    if (!handle_handshake(client_socket)) {
                        close(client_socket);
                        continue;
                    }

                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = client_socket;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event) == -1) {
                        perror("epoll_ctl: add");
                        close(client_socket);
                    } else {
                        std::lock_guard<std::mutex> guard(queue_mutex);
                        clients[client_socket] = client_addr;
                    }
                }
            } else {
                std::lock_guard<std::mutex> guard(queue_mutex);
                work_queue.push(fd);
                cv.notify_one();
            }
        }
    }

    close(server_socket);
    close(epoll_fd);

    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}




