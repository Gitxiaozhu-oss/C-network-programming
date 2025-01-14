#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1472 // MTU size minus IP and UDP headers (1500 - 20 - 8 = 1472)

std::mutex mtx;
std::queue<std::pair<char*, int>> packet_queue;

void receive_packets(int server_socket, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);
    while (true) {
        ssize_t bytes_received = recvfrom(server_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_received > 0) {
            char* data = new char[bytes_received];
            std::memcpy(data, buffer, bytes_received);
            {
                std::lock_guard<std::mutex> lock(mtx);
                packet_queue.push({data, static_cast<int>(bytes_received)});
            }
        }
    }
}

void save_file(const std::string& filename) {
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    std::vector<char*> packets;
    std::vector<int> sizes;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!packet_queue.empty()) {
                packets.push_back(packet_queue.front().first);
                sizes.push_back(packet_queue.front().second);
                packet_queue.pop();
            } else {
                continue;
            }
        }

        outfile.write(packets.back(), sizes.back());
        delete[] packets.back();
        packets.pop_back();
        sizes.pop_back();

        if (sizes.back() < BUFFER_SIZE) {
            break;
        }
    }

    outfile.close();
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;

    // Creating socket file descriptor
    if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // Filling server information
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    std::thread receiver(receive_packets, server_socket, client_addr);
    receiver.detach();

    save_file("received_file.bin");

    close(server_socket);
    return 0;
}




