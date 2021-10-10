#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUF_SIZE 1024
#define USER_LIMIT 5                    // 最大用户数量
#define FD_LIMIT 65535                  // 文件描述符数量限制

// 客户数据
typedef struct client_data
{
    struct sockaddr_in address;
    char* write_buf;
    char read_buf[BUF_SIZE];
}client_data;

// 设置非阻塞socket
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

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
    
    // 创建监听socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);  // 选择流服务(TCP)
    assert(listenfd >= 0);
    
    // 命名socket
    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    // 监听socket
    ret = listen(listenfd, 5);                      // 最大连接数默认为5
    assert(ret != -1);
    
    // 创建users数组
    client_data* users = (client_data*)malloc(sizeof(client_data) * FD_LIMIT);

    // 使用poll(IO复用)同时处理监听socket和已连接的socket
    struct pollfd fds[USER_LIMIT+1];
    int user_cnt = 0;           // 记录已连接的用户数
    int i, j;
    for(i = 1; i <= USER_LIMIT; ++i) {
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = listenfd;       // 处理监听socket
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while(1) {
        ret = poll(fds, user_cnt+1, -1);
        if(ret < 0) {
            printf("poll fail");
            break;
        }
        
        for(i = 0; i < user_cnt+1; ++i) {
            // 处理新的连接
            if(fds[i].fd == listenfd && (fds[i].revents & POLLIN)) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                
                if(connfd < 0) {
                    printf("error is %d\n", errno);
                    continue;
                }
                // 若请求太多，则关闭新到的连接
                if(user_cnt >= USER_LIMIT) {
                    const char* info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                user_cnt++;
                users[connfd].address = client_address;
                setnonblocking(connfd);         // 将新来的连接socket设成非阻塞
                fds[user_cnt].fd = connfd;      // 并添加到poll的事件源中
                fds[user_cnt].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_cnt].revents = 0;
                printf("comes a new user, now have %d users\n", user_cnt);                
            }
            // 处理其他已经连接的客户端的事件
            else if(fds[i].revents & POLLERR) {
                printf("get an error from %d\n", fds[i].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
                    printf("get socket option failed\n");
                }
                continue;
            }
            else if(fds[i].revents & POLLRDHUP) {   // 如果客服端关闭了
                users[fds[i].fd] = users[fds[user_cnt].fd];
                close(fds[i].fd);                   // 服务的也关闭对应的连接
                fds[i] = fds[user_cnt];
                i--;
                user_cnt--;
                printf("a client left\n");
            }
            else if(fds[i].revents & POLLIN) {      // 如果客户端给服务端发来消息
                int connfd = fds[i].fd;
                memset(users[connfd].read_buf, '\0', BUF_SIZE);
                ret = recv(connfd, users[connfd].read_buf, BUF_SIZE-1, 0);
                printf("get %d bytes of client data '%s' from %d\n", ret, users[connfd].read_buf, connfd);
                if(ret < 0) {   // 因为是非阻塞的，所以recv会立即返回，此时是可能出现ret < 0的
                    // 如果读操作出错，则关闭连接
                    if(errno != EAGAIN) {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_cnt].fd];
                        fds[i] = fds[user_cnt];
                        i--;
                        user_cnt--;
                    }               
                }
                else if(ret > 0) {
                    // 如果收到客户端数据，则转发给其他客户端
                    for(j = 1; j <= user_cnt; ++j) {
                        if(fds[j].fd == connfd) continue;
                        fds[j].events |= ~POLLIN;       // 暂不接收其他客户端发来的消息，否则本次客户端发来的消息会被冲掉
                        fds[j].events |= POLLOUT;       // 通知其他客户端准备开始写数据
                        users[fds[j].fd].write_buf = users[connfd].read_buf;
                    }
                }
            }
            else if(fds[i].revents & POLLOUT) {     // 如果服务器要给客户端转发消息
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf) continue;
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                // 给其他客户端转发完数据后，要重新注册fds[i]上的可读事件
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    
    free(users);
    close(listenfd);
    return 0;
}