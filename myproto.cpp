#include <iostream>
#include <stdlib.h>
#include "myproto.h"

using namespace std;

//----------------------------------公共函数----------------------------------
//打印协议数据信息
void printMyProtoMsg(MyProtoMsg& msg)
{
	Json::FastWriter fwriter;
	string jsonStr = fwriter.write(msg.body);

	printf("Head[version=%d,magic=%d,server=%d,len=%d]\n"
		"Body:%s",msg.head.version,msg.head.magic,msg.head.server,
		msg.head.len,jsonStr.c_str());
}

//----------------------------------协议头封装函数----------------------------------
//pData指向一个新的内存，需要pMsg中数据对pData进行填充
void MyProtoEncode::headEncode(uint8_t* pData,MyProtoMsg* pMsg)
{
	//设置协议头版本号为1
	*pData = 1;
	++pData; //向前移动一个字节位置到魔数

	//设置协议头魔数
	*pData = MY_PROTO_MAGIC; //用于简单校验数据，只要发送方和接受方的魔数号一致，则接受认为数据正常
	++pData; //向前移动一个字节位置，到server服务字段（16位大小）

	//设置协议服务号，服务号，用于标识协议中的不同服务，比如向服务器获取get 设置set 添加add ... 都是不同服务（由我们指定）
	//外部设置，存放在pMsg中，其实可以不用修改，直接跳过该地址
	*(uint16_t*)pData = pMsg->head.server; //原文是打算转换为网络字节序（但是没必要）网络中不会查看应用层数据的
	pData+=2; //向前移动两个字节，到len长度字段

	//设置协议头长度字段（协议头+协议消息体），其实在消息体编码中已经被修正了，这里也可以直接跳过
	*(uint32_t*)pData = pMsg->head.len; //原文也是进行了字节序转化，无所谓了。反正IP网络层也不看
}

//协议消息体封装函数：传入的pMsg里面只有部分数据，比如Json协议体，服务号，版本号，我们对消息编码后会修改长度信息，这时需要重新编码协议
//len返回长度信息，用于后面socket发送数据
uint8_t* MyProtoEncode::encode(MyProtoMsg* pMsg, uint32_t& len)
{
	uint8_t* pData = NULL; //用于开辟新的空间，存放编码后的数据
	Json::FastWriter fwriter; //读取Json::Value数据，转换为可以写入文件的字符串

	//协议Json体序列化
	string bodyStr = fwriter.write(pMsg->body);

	//计算消息序列化以后的新长度
	len = MY_PROTO_HEAD_SIZE + (uint32_t)bodyStr.size();
	pMsg->head.len = len; //一会编码协议头部时，会用到
	//申请一块新的空间，用于保存消息（这里可以不用，直接使用原来空间也可以）
	pData = new uint8_t[len];
	//编码协议头
	headEncode(pData,pMsg); //函数内部没有通过二级指针修改pData的数据，修改的是临时数据
	//打包协议体
	memcpy(pData+MY_PROTO_HEAD_SIZE,bodyStr.data(),bodyStr.size());

	return pData; //返回消息首部地址
}


//----------------------------------协议解析类----------------------------------
//初始化协议解析状态
void MyProtoDecode::init()
{
	mCurParserStatus = ON_PARSER_INIT;
}

//清空解析好的消息队列
void MyProtoDecode::clear()
{
	MyProtoMsg* pMsg=NULL;
	while(!mMsgQ.empty())
	{
		pMsg = mMsgQ.front();
		delete pMsg;
		mMsgQ.pop();
	}
}

//判断解析好的消息队列是否为空
bool MyProtoDecode::empty()
{
	return mMsgQ.empty();
}

//出队一个消息
void MyProtoDecode::pop()
{
	mMsgQ.pop();
}  

//获取一个解析好的消息
MyProtoMsg* MyProtoDecode::front()
{
	return mMsgQ.front();
}

//从网络字节流中解析出来协议消息,len由socket函数recv返回
bool MyProtoDecode::parser(void* data,size_t len)
{
	if(len<=0)
		return false;

	uint32_t curLen = 0; //用于保存未解析的网络字节流长度（是对vector)
	uint32_t parserLen = 0; //保存vector中已经被解析完成的字节流，一会用于清除vector中数据
	uint8_t* curData = NULL; //指向data,当前未解析的网络字节流

	curData = (uint8_t*)data;
	
	//将当前要解析的网络字节流写入到vector中	
	while(len--)
	{
		mCurReserved.push_back(*curData);
		++curData;
	}

	curLen = mCurReserved.size();
	curData = (uint8_t*)&mCurReserved[0]; //获取数据首地址

	//只要还有未解析的网络字节流，就持续解析
	while(curLen>0)
	{
		bool parserBreak = false;

		//解析头部
		if(ON_PARSER_INIT == mCurParserStatus || //注意：标识很有用，当数据没有完全达到，会等待下一次接受数据以后继续解析头部
			ON_PARSER_BODY == mCurParserStatus) //可以进行头部解析
		{
			if(!parserHead(&curData,curLen,parserLen,parserBreak))
				return false;
			if(parserBreak)
				break; //退出循环，等待下一次数据到达，一起解析头部
		}
		
		//解析完成协议头，开始解析协议体
		if(ON_PARSER_HEAD == mCurParserStatus)
		{
			if(!parserBody(&curData,curLen,parserLen,parserBreak))
				return false;
			if(parserBreak)
				break;
		}

		//如果成功解析了消息，就把他放入消息队列
		if(ON_PARSER_BODY == mCurParserStatus)
		{
			MyProtoMsg* pMsg = NULL;
			pMsg = new MyProtoMsg;
			*pMsg = mCurMsg;
			mMsgQ.push(pMsg);
		}

		if(parserLen>0)
		{
			//删除已经被解析的网络字节流
			mCurReserved.erase(mCurReserved.begin(),mCurReserved.begin()+parserLen);
		}

		return true;
	}
}

//用于解析消息头
bool MyProtoDecode::parserHead(uint8_t** curData,uint32_t& curLen,
	uint32_t& parserLen,bool& parserBreak)
{
	if(curLen < MY_PROTO_HEAD_SIZE)
	{
		parserBreak = true; //由于数据没有头部长，没办法解析，跳出即可
		return true; //但是数据还是有用的，我们没有发现出错，返回true。等待一会数据到了，再解析头部。由于标志没变，一会还是解析头部
	}

	uint8_t* pData = *curData;
	
	//从网络字节流中，解析出来协议格式数据。保存在MyProtoMsg mCurMsg; //当前解析中的协议消息体
	//解析出来版本号
	mCurMsg.head.version = *pData;
	pData++;
	//解析出用于校验的魔数
	mCurMsg.head.magic = *pData;
	pData++;

	//判断校验信息
	if(MY_PROTO_MAGIC != mCurMsg.head.magic)
		return false; //数据出错

	//解析服务号
	mCurMsg.head.server = *(uint16_t*)pData;
	pData+=2;

	//解析协议消息体长度
	mCurMsg.head.len = *(uint32_t*)pData;

	//判断数据长度是否超过指定的大小
	if(mCurMsg.head.len > MY_PROTO_MAX_SIZE)
		return false;

	//将解析指针向前移动到消息体位置,跳过消息头大小
	(*curData) += MY_PROTO_HEAD_SIZE;
	curLen -= MY_PROTO_HEAD_SIZE;
	parserLen += MY_PROTO_HEAD_SIZE;
	mCurParserStatus = ON_PARSER_HEAD;

	return true;
}

//用于解析消息体
bool MyProtoDecode::parserBody(uint8_t** curData,uint32_t& curLen,
	uint32_t& parserLen,bool& parserBreak)
{
	uint32_t JsonSize = mCurMsg.head.len - MY_PROTO_HEAD_SIZE; //消息体的大小
	if(curLen<JsonSize)
	{
		parserBreak = true; //数据还没有完全到达，我们还要等待一会数据到了，再解析消息体。由于标志没变，一会还是解析消息体
		return true;
	}

	Json::Reader reader; //Json解析类
	if(!reader.parse((char*)(*curData),
		(char*)((*curData)+JsonSize),mCurMsg.body,false)) //false表示丢弃注释
		return false; //解析数据到body中

	//数据指针向前移动
	(*curData)+=JsonSize;
	curLen -= JsonSize;
	parserLen += JsonSize;
	mCurParserStatus = ON_PARSER_BODY;

	return true;
}
