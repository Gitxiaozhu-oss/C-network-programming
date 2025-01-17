cmake_minimum_required(VERSION 3.10)
project(NetworkServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 pthread 库
find_package(Threads REQUIRED)

add_executable(server server.cpp)
target_link_libraries(server Threads::Threads)

add_executable(client client.cpp)
target_link_libraries(client Threads::Threads)
