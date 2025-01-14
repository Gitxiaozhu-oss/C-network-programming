# C++ 网络编程学习仓库
欢迎来到这个C++网络编程学习仓库！本仓库旨在帮助开发者和学生深入理解网络编程的基本概念和应用，提供各种示例代码和实践项目，涵盖从基础到高级的网络编程技术。

## 目录

- [介绍](#介绍)
- [环境要求](#环境要求)
- [项目结构](#项目结构)
- [使用说明](#使用说明)
- [示例代码](#示例代码)
- [贡献](#贡献)
- [许可证](#许可证)

## 介绍

在这个仓库中，您将学习到以下内容：

- TCP和UDP协议的基本概念
- 套接字编程
- 客户端-服务器模型
- 多线程网络编程
- 数据传输和文件传输实现
- 网络安全基础

## 环境要求

为了运行本项目，您需要以下环境：

- C++编译器（如 g++, clang）
- CMake（可选，用于构建项目）
- Git（用于克隆仓库）

## 项目结构

```
├── tcp_file_transfer
│   ├── 2.bin
│   ├── generate_file.py
│   ├── pi.bin
│   ├── tcp_client
│   ├── tcp_client.cpp
│   ├── tcp_server
│   └── tcp_server.cpp
└── udp_file_transfer
    ├── generate_file.py
    ├── pi.bin
    ├── received_file.bin
    ├── udp_client
    ├── udp_client.cpp
    ├── udp_server
    └── udp_server.cpp
```

## 使用说明

1. 克隆本仓库：
   ```bash
   git clone https://github.com/Gitxiaozhu-oss/C-network-programming.git
   cd C-network-programming
   ```

2. 编译示例代码：
   ```bash
   g++ -o tcp_server udp_file_transfer/tcp_server.cpp -lpthread
   g++ -o tcp_client udp_file_transfer/tcp_client.cpp -lpthread
   ```

3. 运行TCP服务器：
   ```bash
   ./tcp_server 1.bin
   ```

4. 在另一个终端运行TCP客户端：
   ```bash
   ./tcp_client 2.bin
   ```

## 示例代码

本仓库包含多个示例代码，展示了如何实现基本的TCP和UDP通信。您可以在 `udp_file_transfer` 目录中找到这些示例。

## 贡献

欢迎任何形式的贡献！如果您有好的想法或改进建议，请提交问题或拉取请求。

## 许可证

本项目采用 MIT 许可证。有关详细信息，请查看 [LICENSE](LICENSE) 文件。

---

感谢您的访问，希望您在学习C++网络编程的过程中获得乐趣与收获！如果您有任何问题，请随时联系我。

Citations:
[1] https://github.com/Gitxiaozhu-oss/C-network-programming
