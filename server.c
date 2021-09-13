#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUF_SIZE 1024

int main(int argc, char* argv[]) {
    if(argc <= 2) {                     // 检查命令行输入是否合法
        printf("usage: %s ip_address port_address \n", argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    // 创建一个IPv4 socket(待监听)地址(具体数据是从用户命令行读入的)
    struct sockaddr_in address;
    bzero(&address, sizeof(address));            // 将一段内存内容全清为零
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);   // 将ip转成网络字节序(大端法)
    address.sin_port = htons(port);              // 将端口转成从主机字节序(小端法)转成网络字节序
    
    // 创建socket
    int sock = socket(PF_INET, SOCK_STREAM, 0);  // 选择流服务(TCP)
    assert(sock >= 0);
    
    // 命名socket
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    // 监听socket
    ret = listen(sock, 5);                      // 最大连接数默认为5
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0) {
        printf("error is %d\n", errno);
    }
    else {
        char buffer[BUF_SIZE];
        memset(buffer, '\0', BUF_SIZE);         // 将buffer指向的内存初始化为\0;

        ret = recv(connfd, buffer, BUF_SIZE-1, 0);
        printf("From client: ‘%s’\n", buffer);

        const char* msg = "Hello, I am the server. I can hear you, and can you hear me?";
        send(connfd, msg, strlen(msg), 0);

        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, 0);
        printf("From client: ‘%s’\n", buffer);
        
        close(connfd);
    }
    
    close(sock);
    return 0;
}