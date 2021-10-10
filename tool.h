#ifndef _TOOL_H_
#define _TOOL_H_

#include <arpa/inet.h>
#include <string.h>

// 创建socket
int get_socket(char* ip, int port) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));                 // 将一段内存内容全清为零

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);     // 将ip转成网络字节序(大端法)
    addr.sin_port = htons(port);                // 将端口转成从主机字节序(小端法)转成网络字节序
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);       //  选择流服务(TCP)
    
    return sockfd;
}

#endif
