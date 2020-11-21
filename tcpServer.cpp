#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "myproto.h"

int startup(char* _port,char* _ip);
int myprotoRecv(int sock,char* buf,int max_len);

int main(int argc,char* argv[])
{
	if(argc!=3)
	{
		printf("Usage:%s local_ip local_port\n",argv[0]);
		return 1;
	}

	//获取监听socket信息
	int listen_sock = startup(argv[2],argv[1]); 

	//设置结构体，用于接收客户端的socket地址结构体
	struct sockaddr_in remote;
	unsigned int len = sizeof(struct sockaddr_in);

	while(1)
	{
		//开始阻塞方式接收客户端链接
		int sock = accept(listen_sock,(struct sockaddr*)&remote,&len);
		if(sock<0)
		{
			printf("client accept failure!\n");
			continue;
		}
		//开始接收客户端消息
		printf("get connect from %s:%d\n",inet_ntoa(remote.sin_addr),ntohs(remote.sin_port)); //inet_ntoa将网络地址转换成“.”点隔的字符串格式
		char buf[1024];

		len = myprotoRecv(sock,buf,1024); //len复用，这里作为接收长度------这里可以改为循环
		
		close(sock);
	}
	return 0;
}

int startup(char* _port,char* _ip)
{
	int sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sock < 0)
	{
		printf("socket create failure!\n");
		exit(-1);
	}

	//绑定服务端的地址信息，用于监听当前服务的某网卡、端口
	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(atoi(_port));
	local.sin_addr.s_addr = inet_addr(_ip);

	int len = sizeof(local);

	if(bind(sock,(struct sockaddr*)&local,len)<0)
	{
		printf("socket bind failure!\n");
		exit(-2);
	}

	//开始监听sock,设置同时并发数量
	if(listen(sock,5)<0) //允许最大连接数量5
	{
		printf("socket listen failure!\n");
		exit(-3);
	}

	return sock; //返回文件句柄
}

int myprotoRecv(int sock,char* buf,int max_len)
{
	unsigned int len;

	len = recv(sock,buf,sizeof(char)*max_len,0);

	MyProtoDecode myDecode;
	myDecode.init();

	if(!myDecode.parser(buf,len))
	{
		cout<<"parser msg failed!"<<endl;
	}
	else
	{
		cout<<"parser msg successful!"<<endl;
	}

	//------解析消息
	MyProtoMsg* pMsg = NULL;

	while(!myDecode.empty())
	{
		pMsg = myDecode.front();
		printMyProtoMsg(*pMsg);
		myDecode.pop();
	}

	return len;
}


/*
inet_addr 将字符串形式的IP地址 -> 网络字节顺序  的整型值

inet_ntoa 网络字节顺序的整型值 ->字符串形式的IP地址
*/