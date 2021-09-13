#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 1024

int main(int argc, char* argv[]) {
    if(argc <= 2) {                     // 检查命令行输入是否合法
        printf("usage: %s ip_address port_address \n", argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    // 创建一个IPv4 socket(目的)地址(具体数据是从用户命令行读入的)
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));     // 将一段内存内容全清为零
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);   // 将ip转成网络字节序(大端法)
    server_address.sin_port = htons(port);               // 将端口转成从主机字节序(小端法)转成网络字节序
    
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);       //  选择流服务(TCP)
    assert(sockfd >= 0);
    if(connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        printf("connect failed\n");
    }
    else {
        const char* msg = "Hello server, I am the client. Can you hear me?";
        send(sockfd, msg, strlen(msg), 0);            // send函数将数据从应用层复制到socket的缓冲区(位于内核)
                                                        // 之后再由传输层协议将数据发出
        char buffer[BUF_SIZE];
        memset(buffer, '\0', BUF_SIZE);                 // 将buffer指向的内存初始化为\0;
        
        int ret = recv(sockfd, buffer, BUF_SIZE-1, 0);
        printf("From server: ‘%s’\n", buffer);

        const char* msg2 = "I can hear you. Let's chat.";
        send(sockfd, msg2, strlen(msg2), 0);
    }                                                   
    
    close(sockfd);
    return 0;
}