#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<arpa/inet.h>
#include<signal.h>
#include<stdbool.h>
#include<sys/uio.h>

//_b是好的代码，后面加入群聊天
//服务端和客户端都写到了：群发判断，通过receive_name来搞，特定一个名字

#define MAX_CLIENT 1000//最大的客户端连接数
#define BUFFER_SIZE 1024//最大的缓冲区大小
#define USER_NAME_MAX 100//用户名最大
#define INF_MAX 2048//消息最大

struct message {//消息结构
    char send_name[USER_NAME_MAX];
    char receive_name[USER_NAME_MAX];
    char chat_information[INF_MAX];
};

struct users {//临时存储当时链接的用户
    char usersname[2][USER_NAME_MAX]; // 0: IP, 1: Username
    int fd;
    bool connected;
};

int server_fd, client_fd;//服务端fd和while中当前新的客户端fd
int client_fds[MAX_CLIENT] = {0};//临时存储当时链接的用户的fd
struct users us[MAX_CLIENT];//临时存储当时链接的用户
int now_users_num = 0;//当前链接的用户数量
struct sockaddr_in server_addr, client_addr;//服务端和客户端属性
socklen_t addr_len = sizeof(client_addr);

void sig_handler(int signo) {
    if (signo == SIGINT) {
        printf("Server closing...\n");
        close(server_fd);
        for (int i = 0; i < MAX_CLIENT; i++) {
            if (client_fds[i] != 0) close(client_fds[i]);
            client_fds[i]=0;
        }
        exit(0);
    }
}

void Main_control();
void Do_one_talk(int fd,struct message *msg,struct iovec *iov);//一对一聊天
void Do_together_talk(int fd,int max_fd,struct message *msg,struct iovec *iov);//群聊天，这个服务器就是群管理，直接发给所有人

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    int port = atoi(argv[1]);
    

    // 创建并配置服务器 socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    //设置属性允许在同一端口上重复绑定套接字
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    //设置服务端端口，接收的来源
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d...\n", port);

    Main_control();

    close(server_fd);
    return 0;
}

void Main_control()
{
    fd_set read_fds, temp_fds;
    int max_fd = server_fd;

    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);

    while (1) {
        temp_fds = read_fds;

        if (select(max_fd + 1, &temp_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &temp_fds)) {
                if (fd == server_fd) { // 新连接
                    //判断有没有满
                    if(now_users_num>=MAX_CLIENT)
                    {
                        char full_msg[]="ERROR:Server full,try late...";
                        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                        send(client_fd, full_msg, strlen(full_msg), 0);
                        close(client_fd);
                        continue;
                    }
                    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
                        perror("accept");
                        continue;
                    }
                    printf("New connection from %s:%d (fd=%d)\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);

                    us[now_users_num].fd = client_fd;
                    us[now_users_num].connected = true;
                    char client_ip[16];
                    memset(client_ip, 0, sizeof(client_ip));
                    inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip));
                    strcpy(us[now_users_num].usersname[0], client_ip);

                    // 检查用户是否存在
                    FILE *fp = fopen("users.txt", "r");
                    if (fp == NULL) {
                        perror("fopen");
                        close(client_fd);
                        continue;
                    }

                    char name[USER_NAME_MAX], file_ip[16];
                    bool user_found = false;
                    while (fscanf(fp, "%s %s", name, file_ip) == 2) {
                        if (strcmp(file_ip, client_ip) == 0) {
                            strcpy(us[now_users_num].usersname[1], name);
                            user_found = true;
                            break;
                        }
                    }
                    fclose(fp);

                    if (user_found) {
                        // 添加到客户端列表
                        for (int i = 0; i < MAX_CLIENT; i++) {
                            if (client_fds[i] == 0) {
                                client_fds[i] = client_fd;
                                FD_SET(client_fd, &read_fds);
                                if (client_fd > max_fd) max_fd = client_fd;
                                break;
                            }
                        }
                        now_users_num++;
                        char welcome_msg[] = "Welcome! You can start chatting.";
                        send(client_fd, welcome_msg, strlen(welcome_msg) + 1, 0);
                        printf("User %s connected\n", us[now_users_num - 1].usersname[1]);
                    } 
                    else {
                        // 用户不存在，发送错误码并关闭连接
                        //或者说让用户创建，先等待用户传来的信息再判断要不要创建
                        char error_msg[] = "ERROR: Not registered user";
                        send(client_fd, error_msg, strlen(error_msg) + 1, 0);
                        char new_buffer[BUFFER_SIZE];
                        memset(new_buffer,0,sizeof new_buffer);
                        if (recv(client_fd, new_buffer, sizeof(new_buffer), 0) <= 0) {
                            close(client_fd);
                            printf("Connection from %s rejected (no response)\n", client_ip);
                            continue;
                        }

                        if(strcmp(new_buffer,"Create")==0)
                        {
                            char new_name[BUFFER_SIZE];
                            memset(new_name,0,sizeof new_name);
                            if (recv(client_fd, new_name, sizeof(new_name), 0) <= 0) {
                                close(client_fd);
                                printf("Connection from %s rejected (no name)\n", client_ip);
                                continue;
                            }
                            new_name[strcspn(new_name, "\n")] = '\0';
                            char can_not_name[BUFFER_SIZE]="You can't use 'everyone' as your name.";
                            if(strcmp(new_name,"everyone")==0)
                            {
                                send(fd,can_not_name,sizeof(can_not_name),0);
                                close(client_fd);
                                printf("Connection from %s rejected (not registered)\n", client_ip);
                                continue;
                            }
                            FILE *new_fp;
                            if((new_fp=fopen("users.txt","a+"))==NULL)
                            {
                                perror("fopen");
                                close(client_fd);
                                exit(1);
                            }
                            char new_ip[16];
                            inet_ntop(AF_INET,&client_addr.sin_addr.s_addr,new_ip,sizeof(new_ip));
                            fprintf(fp,"%s %s\n",new_name,new_ip);
                            fclose(new_fp);

                            //写入文件后 添加到客户端列表
                            for (int i = 0; i < MAX_CLIENT; i++) {
                                if (client_fds[i] == 0) {
                                    client_fds[i] = client_fd;
                                    FD_SET(client_fd, &read_fds);
                                    if (client_fd > max_fd) max_fd = client_fd;
                                    break;
                                }
                            }
                            strcpy(us[now_users_num].usersname[1],new_name);
                            strcpy(us[now_users_num].usersname[0],new_ip);
                            us[now_users_num].fd=client_fd;
                            us[now_users_num].connected=1;
                            now_users_num++;
                            char welcome_msg[] = "Welcome! You can start chatting.";
                            send(client_fd, welcome_msg, strlen(welcome_msg) + 1, 0);                        
                            printf("User %s connected\n", us[now_users_num - 1].usersname[1]);
                        }
                        else
                        {
                            close(client_fd);
                            printf("Connection from %s rejected (not registered)\n", client_ip);
                        }
                    }
                } 
                else { // 客户端数据
                    struct message msg;
                    struct iovec iov;
                    iov.iov_base = &msg;
                    iov.iov_len = sizeof(msg);

                    ssize_t bytes_received = readv(fd, &iov, 1);
                    if (bytes_received <= 0 || strcmp(msg.chat_information,"I break...")==0) { // 客户端断开连接
                        printf("Client disconnected (fd=%d)\n", fd);
                        close(fd);
                        FD_CLR(fd, &read_fds);

                        // 从用户列表中移除
                        for (int i = 0; i < now_users_num; i++) {
                            if (us[i].fd == fd) {
                                us[i].connected = false;
                                for (int j = i; j < now_users_num - 1; j++) {
                                    us[j] = us[j + 1];
                                }
                                now_users_num--;
                                break;
                            }
                        }

                        // 更新客户端 fd 数组
                        for (int i = 0; i < MAX_CLIENT; i++) {
                            if (client_fds[i] == fd) {
                                client_fds[i] = 0;
                                break;
                            }
                        }
                    } 
                    else { // 转发消息
                        // 查找接收方
                        //判断是不是群发
                        if(strcmp(msg.receive_name,"everyone")==0)
                        {
                            Do_together_talk(fd,max_fd,&msg,&iov);
                        }
                        else
                        {
                            Do_one_talk(fd,&msg,&iov);
                        }
                        
                    }
                }
            }
        }
    }
}

void Do_one_talk(int fd,struct message *msg,struct iovec *iov)
{
    // 填充发送方信息
    for (int i = 0; i < now_users_num; i++) {
        if (us[i].fd == fd) {
            strcpy(msg->send_name, us[i].usersname[1]);
            break;
        }
    }
    bool receiver_found = false;
    int receiver_fd = -1;
    for (int i = 0; i < now_users_num; i++) {
        if (strcmp(us[i].usersname[1], msg->receive_name) == 0 && us[i].connected) {
            receiver_found = true;
            receiver_fd = us[i].fd;
            break;
        }
    }

    if (receiver_found) {
        writev(receiver_fd, iov, 1);
    } else {
        char error_msg[] = "ERROR: Receiver not found or offline";
        send(fd, error_msg, strlen(error_msg) + 1, 0);
    }
}

void Do_together_talk(int fd,int max_fd,struct message *msg,struct iovec *iov)
{
    for (int i = 0; i < now_users_num; i++) 
    {
        if (us[i].fd == fd) 
        {
            strcpy(msg->send_name, us[i].usersname[1]);
            break;
        }
    }
    for(int i=0;i<now_users_num;++i)
    {
        if(us[i].fd!=fd)//不是自己就发
        {
            writev(us[i].fd, iov, 1);
        }
    }
}
