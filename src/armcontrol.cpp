#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <uarm/uarm.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <errno.h>

#define PORT 8801
bool runFlag = true;
std::vector<pthread_t> threadIds;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct ThreadArgs {
    int socket;
    uarm::Swift *swift;
};

const int BUFFER_SIZE = 1024;

enum Command {
    DISCONNECT,
    EXIT,
    VERIFY,
    GET_POS_X,
    GET_POS_Y,
    GET_POS_Z,
    RESET,
    ANGLE_1ST,
    ANGLE_2ND,
    ANGLE_3RD,
    NOISE,
    UNKNOWN
};

Command parseCommand(const std::string& input, float& angle) {
    size_t plusPos = input.find('+');
    if (plusPos != std::string::npos) {
        std::string flag_angle = input.substr(0, plusPos);
        std::string angleStr = input.substr(plusPos + 1);
        angle = std::stof(angleStr);
        if (flag_angle == "angle1st") return ANGLE_1ST;
        if (flag_angle == "angle2nd") return ANGLE_2ND;
        if (flag_angle == "angle3rd") return ANGLE_3RD;
    } else {
        if (input == "disconnect") return DISCONNECT;
        if (input == "exit") return EXIT;
        if (input == "verify") return VERIFY;
        if (input == "get_pos_x") return GET_POS_X;
        if (input == "get_pos_y") return GET_POS_Y;
        if (input == "get_pos_z") return GET_POS_Z;
        if (input == "reset") return RESET;
        if (input == "noise") return NOISE;
    }
    return UNKNOWN;
}

void sendMessage(int clientSocket, const std::string& message) {
    send(clientSocket, message.c_str(), message.length(), 0);
    std::cout << "Sent: " << message << std::endl;
}

void *clientThread(void *arg) {
    bool is_running = true;
    std::unique_ptr<ThreadArgs> threadArgs(static_cast<ThreadArgs*>(arg));
    int clientSocket = threadArgs->socket;
    uarm::Swift *swift = threadArgs->swift;

    while (is_running) {
        char buffer[BUFFER_SIZE] = {0};
        float angle;
        int valread = read(clientSocket, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            if (valread == 0) {
                std::cout << "Client disconnected" << std::endl;
            } else {
                std::cerr << "Failed to read data: " << strerror(errno) << std::endl;
            }
            break;
        }

        std::string input(buffer);
        Command command = parseCommand(input, angle);

        switch (command) {
            case DISCONNECT:
                break;
            case EXIT:
                runFlag = false;
                break;
            case VERIFY:
                sendMessage(clientSocket, "OK");
                break;
            case GET_POS_X:
            case GET_POS_Y:
            case GET_POS_Z: {
                pthread_mutex_lock(&mutex);
                std::vector<float> positions = swift->get_position();
                float value = positions[command - GET_POS_X];
                sendMessage(clientSocket, std::to_string(value));
                pthread_mutex_unlock(&mutex);
                break;
            }
            case RESET:
                pthread_mutex_lock(&mutex);
                system("python3 /home/spark/request_spark/armcontrol/scripts/reset.py");
                pthread_mutex_unlock(&mutex);
                std::cout << "status: Reset OK!!" << std::endl;
                break;
            case ANGLE_1ST:
            case ANGLE_2ND:
            case ANGLE_3RD: {
                pthread_mutex_lock(&mutex);
                swift->set_servo_angle(command - ANGLE_1ST, angle, 100);
                pthread_mutex_unlock(&mutex);
                std::cout << "status: Set OK, angle = " << angle << std::endl;
                break;
            }
            case NOISE:
                pthread_mutex_lock(&mutex);
                swift->set_buzzer(250, 25);
                pthread_mutex_unlock(&mutex);
                break;
            default:
                sendMessage(clientSocket, "Hello from server");
                is_running = false;
                break;
        }

        if (command == DISCONNECT || command == EXIT)
            is_running = false;;
    }

    close(clientSocket);
    pthread_exit(NULL);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "无法创建套接字: " << strerror(errno) << std::endl;
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "设置套接字选项失败: " << strerror(errno) << std::endl;
        close(server_fd);
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        std::cerr << "绑定套接字失败: " << strerror(errno) << " (错误代码: " << errno << ")" << std::endl;
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 3) == -1) {
        std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
        close(server_fd);
        return -1;
    }

    uarm::Swift swift("/dev/ttyACM0");

    while (runFlag) {
        int addrlen = sizeof(address);
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket == -1) {
            std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            continue;
        }

        ThreadArgs *threadArgs = new ThreadArgs{new_socket, &swift};
        pthread_t threadId;
        if (pthread_create(&threadId, NULL, clientThread, threadArgs) != 0) {
            std::cerr << "Failed to create thread: " << strerror(errno) << std::endl;
            close(new_socket);
            delete threadArgs;
        } else {
            threadIds.push_back(threadId);
        }
    }

    for (pthread_t tid : threadIds) {
        pthread_join(tid, NULL);
    }

    close(server_fd);
    swift.disconnect();
    pthread_mutex_destroy(&mutex);

    return 0;
}
