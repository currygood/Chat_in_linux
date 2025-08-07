# Chat_in_linux
简易的Linux局域网聊天项目，上限只有1k人
先运行服务端。
客户端再连接上去
服务端用法：可执行文件 端口
注意，服务端要打开tcp对应端口
客户端用法：可执行文件 服务端局域网ip 端口
目前只能一对一聊天，后续会弄成群聊
OK，2025-7-31   19:27
更新：测试了很多次没有bug，可以私聊和群发（发给所有连接上服务端的机器）

1.编译文件
gcc -o server server.c
gcc -o client client.c

2.运行好服务端
./server 8888

3.客户端访问
./client server_ip 8888

备注：别的机器访问服务端要把服务端的tcp的对应端口开放
sudo iptables -A INPUT -p tcp --dport 8888 -j ACCEPT
