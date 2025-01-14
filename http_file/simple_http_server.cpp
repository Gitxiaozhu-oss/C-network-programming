#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <map>
#include <thread>
#include <vector>

#define PORT 8080
#define BUFFER_SIZE 4096

// MIME types mapping
std::map<std::string, std::string> mime_types = {
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".mp4", "video/mp4"},
    {".txt", "text/plain"},
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"}
};

// Function to get the MIME type based on file extension
std::string get_mime_type(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        std::string ext = filename.substr(pos);
        auto it = mime_types.find(ext);
        if (it != mime_types.end()) {
            return it->second;
        }
    }
    return "application/octet-stream";
}

// Function to read file content into a string
bool read_file_content(const std::string& path, std::string& content) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return true;
}

// Function to list directory contents
std::string list_directory_contents(const std::string& dir_path) {
    DIR* dir;
    struct dirent* ent;
    std::ostringstream oss;

    if ((dir = opendir(dir_path.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                oss << "<li><a href=\"/" << ent->d_name << "\">" << ent->d_name << "</a></li>";
            }
        }
        closedir(dir);
    } else {
        oss << "<li>No files found</li>";
    }

    return oss.str();
}

// Function to handle client requests
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        std::cerr << "Failed to read request" << std::endl;
        close(client_socket);
        return;
    }

    std::string request(buffer);
    std::istringstream iss(request);
    std::string method, path, http_version;
    iss >> method >> path >> http_version;

    if (method != "GET") {
        std::string response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(client_socket, response.c_str(), response.size(), 0);
        close(client_socket);
        return;
    }

    // Handle root path
    if (path == "/") {
        std::string dir_contents = list_directory_contents("resources");
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/html\r\n"
            << "Content-Length: " << dir_contents.size() + 75 << "\r\n"
            << "Connection: close\r\n\r\n"
            << "<html><body><h1>Resources Directory</h1><ul>"
            << dir_contents
            << "</ul></body></html>";

        std::string response = oss.str();
        send(client_socket, response.c_str(), response.size(), 0);
        close(client_socket);
        return;
    }

    std::string file_path = "resources" + path;
    std::string content;
    if (!read_file_content(file_path, content)) {
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(client_socket, response.c_str(), response.size(), 0);
        close(client_socket);
        return;
    }

    std::string mime_type = get_mime_type(file_path);
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << mime_type << "\r\n"
        << "Content-Length: " << content.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << content;

    std::string response = oss.str();
    send(client_socket, response.c_str(), response.size(), 0);
    close(client_socket);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        // Create a new thread to handle each client request
        std::thread t(handle_client, new_socket);
        t.detach(); // Detach the thread so that it runs independently
    }

    close(server_fd);
    return 0;
}




