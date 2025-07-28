#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<signal.h>
#include<sys/uio.h>
#include<sys/wait.h>

#define USER_NAME_MAX 100//用户名最大
#define INF_MAX 2048//消息最大
#define BUFFER_SIZE 1024//缓冲区最大

struct message {//消息结构
    char send_name[USER_NAME_MAX];
    char receive_name[USER_NAME_MAX];
    char chat_information[INF_MAX];
};

int sockfd;//套接字fd

void sig_handler(int signo) {
    if (signo == SIGINT) {
        printf("\nClient exiting...\n");
        close(sockfd);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    // 创建并连接服务器
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    //设置链接的服务端
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(1);
    }

    //链接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    // 接收服务器响应
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {//服务器挂了
        printf("Server disconnected unexpectedly\n");
        close(sockfd);
        exit(1);
    }

    // 检查是否为错误消息
    if (strstr(buffer, "ERROR:") != NULL) 
    {//根据连接上去的这个客户端的ip判断是不是member,服务端还会判断有没有达到最大连接数
        printf("%s\n", buffer);
        if(strcmp(buffer,"ERROR:Server full,try late...\n")==0)//满了直接退出
        {
            close(sockfd);
            exit(0);
        }
        //然后问要不要注册
        printf("Do you want to create:(enter 1 is create 2 is quit):");
        int choice;
        scanf("%d",&choice);
	while(getchar()!='\n');
	if(choice==1)
        {
            char c[10]="Create";
            send(sockfd,c,sizeof(c),0);//向服务器发送请求
            printf("Ok.Enter your name:");
            memset(buffer,0,sizeof buffer);
            fgets(buffer,sizeof(buffer),stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            send(sockfd,buffer,sizeof(buffer),0);//发送名字
        }
        else
        {
            send(sockfd,"Disconnected",sizeof("Disconnected"),0);
            printf("End...\n");
            close(sockfd);
            exit(1);
        }
        recv(sockfd,buffer,sizeof(buffer),0);
        printf("%s\n", buffer);
    }
    else
    {
        printf("%s\n", buffer);
    }

    

    // 获取聊天对象
    char receiver[USER_NAME_MAX];
    printf("Enter recipient's username: ");
    fgets(receiver, sizeof(receiver), stdin);
    receiver[strcspn(receiver, "\n")] = '\0';//把fgets获取字符串的\n去掉

    // 创建子进程处理接收和发送
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(sockfd);
        exit(1);
    } else if (pid == 0) { // 子进程：接收消息
        while (1) {
            struct message msg;
            struct iovec iov;
            iov.iov_base = &msg;
            iov.iov_len = sizeof(msg);

            ssize_t bytes_received = readv(sockfd, &iov, 1);
            if (bytes_received <= 0) {
                printf("Server disconnected\n");
                close(sockfd);
                exit(0);
            }

            printf("\n%s: %s\n", msg.send_name, msg.chat_information);
            printf("What do you want to send: ");
            fflush(stdout); // 确保提示信息显示
        }
    } else { // 父进程：发送消息
        struct message msg;
        strcpy(msg.receive_name, receiver);

        while (1) {
            printf("What do you want to send: ");
            char message[INF_MAX];
            if (fgets(message, sizeof(message), stdin) == NULL) {
                continue;
            }
            message[strcspn(message, "\n")] = '\0';

            // 检查是否退出
            if (strcmp(message, "quit") == 0) {
                break;
            }

            strcpy(msg.chat_information, message);
            struct iovec iov;
            iov.iov_base = &msg;
            iov.iov_len = sizeof(msg);

            if (writev(sockfd, &iov, 1) < 0) {
                perror("writev");
                break;
            }
        }

        // 清理资源
        close(sockfd);
        waitpid(pid, NULL, 0);
        kill(pid, SIGTERM); // 终止子进程
        printf("Client exited\n");
        return 0;
    }
}