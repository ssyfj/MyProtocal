#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "myproto.h"

int myprotoSend(int sock);

int main(int argc,char* argv[])
{
	if(argc != 3)
	{
		printf("USage:%s ip port\n", argv[0]);
		return 0;
	}

	//开始创建socket
	int sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sock < 0)
	{
		printf("socket create failure\n");
		return -1;
	}

	//使用connect与服务器地址，端口连接，需要定义服务端信息：地址结构体
	struct sockaddr_in server;
	server.sin_family = AF_INET; //IPV4
	server.sin_port = htons(atoi(argv[2])); //atoi将字符串转数字
	server.sin_addr.s_addr = inet_addr(argv[1]); //不直接使用htonl,因为传入的是字符串IP地址，使用inet_addr正好对字符串IP,转网络大端所用字节序

	unsigned int len = sizeof(struct sockaddr_in); //获取socket地址结构体长度

	if(connect(sock,(struct sockaddr*)&server,len)<0)
	{
		printf("socket connect failure\n");
		return -2;
	}

	//连接成功，进行数据发送-------------这里可以改为循环发送
	len = myprotoSend(sock);

	close(sock);
	return 0;
}

int myprotoSend(int sock) //-----------这里改为字符串解析，发送自己解析的Json数据
{

	uint32_t len=0;
	uint8_t* pData = NULL;

	MyProtoMsg msg1;

	MyProtoEncode myEncode;

	//------放入消息
	msg1.head.server = 1;
	msg1.body["op"] = "set";
	msg1.body["key"] = "id";
	msg1.body["value"] = "6666";

	pData = myEncode.encode(&msg1,len);

	return send(sock,pData,len,0);
}