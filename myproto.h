#ifndef __MY_PROTO_H
#define __MY_PROTO_H

#include <stdint.h>
#include <stdio.h>
#include <queue>
#include <vector>
#include <iostream>
#include <cstring>
#include <json/json.h>

using namespace std;

const uint8_t MY_PROTO_MAGIC = 8; //协议魔数：通过魔数进行简单对比校验，也可以像之前学的CRC校验替换
const uint32_t MY_PROTO_MAX_SIZE = 10*1024*1024; //10M协议中数据最大
const uint32_t MY_PROTO_HEAD_SIZE = 8; //协议头大小

typedef enum MyProtoParserStatus //协议解析的状态
{
	ON_PARSER_INIT = 0, //初始状态
	ON_PARSER_HEAD = 1, //解析头部
	ON_PARSER_BODY = 2, //解析数据
}MyProtoParserStatus;

//协议头部
struct MyProtoHead
{
	uint8_t version; //协议版本号
	uint8_t magic; //协议魔数
	uint16_t server; //协议复用的服务号，用于标识协议中的不同服务，比如向服务器获取get 设置set 添加add ... 都是不同服务（由我们指定）
	uint32_t len; //协议长度（协议头部+变长json协议体=总长度）
};

//协议消息体
struct MyProtoMsg
{
	MyProtoHead head; //协议头
	Json::Value body; //协议体
};


//公共函数
//打印协议数据信息
void printMyProtoMsg(MyProtoMsg& msg);

//协议封装类
class MyProtoEncode
{
public:
	//协议消息体封装函数：传入的pMsg里面只有部分数据，比如Json协议体，服务号，我们对消息编码后会修改长度信息，这时需要重新编码协议
	uint8_t* encode(MyProtoMsg* pMsg, uint32_t& len); //返回长度信息，用于后面socket发送数据
private:
	//协议头封装函数
	void headEncode(uint8_t* pData,MyProtoMsg* pMsg);
};



//协议解析类
class MyProtoDecode
{
private:
	MyProtoMsg mCurMsg; //当前解析中的协议消息体
	queue<MyProtoMsg*> mMsgQ; //解析好的协议消息队列
	vector<uint8_t> mCurReserved; //未解析的网络字节流，可以缓存所有没有解析的数据（按字节）
	MyProtoParserStatus mCurParserStatus; //当前接受方解析状态
public:
	void init(); //初始化协议解析状态
	void clear(); //清空解析好的消息队列
	bool empty(); //判断解析好的消息队列是否为空
	void pop();  //出队一个消息

	MyProtoMsg* front(); //获取一个解析好的消息
	bool parser(void* data,size_t len); //从网络字节流中解析出来协议消息，len是网络中的字节流长度，通过socket可以获取
private:
	bool parserHead(uint8_t** curData,uint32_t& curLen,
		uint32_t& parserLen,bool& parserBreak); //用于解析消息头
	bool parserBody(uint8_t** curData,uint32_t& curLen,
		uint32_t& parserLen,bool& parserBreak); //用于解析消息体
};

#endif