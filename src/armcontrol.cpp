#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <uarm/uarm.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>

#define PORT 8801
bool runFlag = true;
std::vector<pthread_t> threadIds;

// 在全局定义一个互斥锁
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// 数据结构用于传递线程参数
struct ThreadArgs {
    int socket;
    uarm::Swift *swift;
};

// 处理客户端连接的线程函数
void *clientThread(void *arg) {
    ThreadArgs *threadArgs = (ThreadArgs *)arg;
    int clientSocket = threadArgs->socket;
    uarm::Swift *swift = threadArgs->swift;

    while (true) {
        // 接收客户端发送的数据
        char buffer[1024] = {0};
        float angle;
        std::string flag_angle;
        int valread = read(clientSocket, buffer, 1024);
        if (valread == -1) {
            std::cerr << "Failed to read data" << std::endl;
            break;
        }

        std::cout << "Received: " << buffer << std::endl;

        std::string input(buffer);
        size_t plusPos = input.find('+');
        if (plusPos != std::string::npos) {
            // 提取指令部分
            flag_angle = input.substr(0, plusPos);
            // 提取角度部分
            std::string angleStr = input.substr(plusPos + 1);
            // 将角度字符串转换为float
            angle = std::stof(angleStr);
        }

        // 根据接收到的消息执行对应操作
        if (strcmp(buffer, "disconnect") == 0) {
            break;
        } else if (strcmp(buffer, "exit") == 0) {
            runFlag = false;
            break;
        } else if (strcmp(buffer, "verify") == 0) {
            // 如果接收到"verify"，返回 "OK"
            std::string message = "OK";
            send(clientSocket, message.c_str(), message.length(), 0);
            std::cout << "Sent: " << message << std::endl;
        } else if (strcmp(buffer, "get_pos_x") == 0) {
            pthread_mutex_lock(&mutex);
            // 如果接收到"get_pos"，返回机械臂位置信息
            std::vector<float> positions = swift->get_position();
            float value = positions[0];
            std::string message = std::to_string(value);
            send(clientSocket, message.c_str(), message.length(), 0);
            pthread_mutex_unlock(&mutex);
            std::cout << "Sent: " << message << std::endl;
        } else if (strcmp(buffer, "get_pos_y") == 0) {
            pthread_mutex_lock(&mutex);
            // 如果接收到"get_pos"，返回机械臂位置信息
            std::vector<float> positions = swift->get_position();
            float value = positions[1];
            std::string message = std::to_string(value);
            send(clientSocket, message.c_str(), message.length(), 0);
            pthread_mutex_unlock(&mutex);
            std::cout << "Sent: " << message << std::endl;
        } else if (strcmp(buffer, "get_pos_z") == 0) {
            pthread_mutex_lock(&mutex);
            // 如果接收到"get_pos"，返回机械臂位置信息
            std::vector<float> positions = swift->get_position();
            float value = positions[2];
            std::string message = std::to_string(value);
            send(clientSocket, message.c_str(), message.length(), 0);
            pthread_mutex_unlock(&mutex);
            std::cout << "Sent: " << message << std::endl;
        } else if (strcmp(buffer, "reset") == 0) {
            pthread_mutex_lock(&mutex);
            // 如果接收到"reset"，则重置机械臂
            system("python3 /home/spark/request_spark/armcontrol/scripts/reset.py");
            swift->set_buzzer(100000,1);
            pthread_mutex_unlock(&mutex);
            std::cout << "status: Reset OK!!" << std::endl;
        } else if (flag_angle == "angle1st") {
            pthread_mutex_lock(&mutex);
            swift->set_servo_angle(0, angle, 100);
            pthread_mutex_unlock(&mutex);
            std::cout << "status: Set OK, angle = " << angle << std::endl;
        } else if (flag_angle == "angle2nd") {
            pthread_mutex_lock(&mutex);
            swift->set_servo_angle(1, angle, 100);
            pthread_mutex_unlock(&mutex);
            std::cout << "status: Set OK!!, angle = " << angle << std::endl;
        } else if (flag_angle == "angle3rd") {
            pthread_mutex_lock(&mutex);
            swift->set_servo_angle(2, angle, 100);
            pthread_mutex_unlock(&mutex);
            std::cout << "status: Set OK!!, angle = " << angle << std::endl;
        } else if (strcmp(buffer, "noise") == 0) {
            pthread_mutex_lock(&mutex);
            swift->set_buzzer(100000,10);
            pthread_mutex_unlock(&mutex);
        } else {
            // 其他情况，返回默认消息
            std::string message = "Hello from server";
            send(clientSocket, message.c_str(), message.length(), 0);
            std::cout << "Sent: " << message << std::endl;
        }
    }

    // 关闭客户端连接的socket
    close(clientSocket);

    delete threadArgs;
    pthread_exit(NULL);
}

int main() {
    // 创建一个socket对象
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "无法创建套接字" << std::endl;
        return -1;
    }

    // 设置套接字选项 SO_REUSEADDR
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "设置套接字选项失败" << std::endl;
        close(server_fd);
        return -1;
    }

    // 定义地址结构
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 绑定socket到地址和端口
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        std::cerr << "绑定套接字失败: " << strerror(errno) << " (错误代码: " << errno << ")" << std::endl;
        close(server_fd);  // 退出前关闭套接字
        return -1;
    }

    // 监听连接请求
    if (listen(server_fd, 3) == -1) {
        std::cerr << "Failed to listen" << std::endl;
        return -1;
    }

    // 创建 uarm::Swift 对象
    uarm::Swift swift("/dev/ttyACM0");

    while (runFlag) {
        // 接受一个连接请求
        int addrlen = sizeof(address);
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket == -1) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        // 创建线程来处理客户端连接
        ThreadArgs *threadArgs = new ThreadArgs;
        threadArgs->socket = new_socket;
        threadArgs->swift = &swift;
        pthread_t threadId;
        if (pthread_create(&threadId, NULL, clientThread, (void *)threadArgs) != 0) {
            std::cerr << "Failed to create thread" << std::endl;
            close(new_socket);
            delete threadArgs;
        } else {
            // 存储子线程的ID
            threadIds.push_back(threadId);
        }
    }

    // 等待所有子线程完成
    for (pthread_t tid : threadIds) {
        pthread_join(tid, NULL);
    }

    // 关闭socket对象
    close(server_fd);
    swift.disconnect();

    return 0;
}