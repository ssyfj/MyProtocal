#include "myproto.h"

int main(int argc,char* argv[])
{
	uint32_t len=0;
	uint8_t* pData = NULL;

	MyProtoMsg msg1;
	MyProtoMsg msg2;

	MyProtoDecode myDecode;
	MyProtoEncode myEncode;

	//------放入第一个消息
	msg1.head.server = 1;
	msg1.body["op"] = "set";
	msg1.body["key"] = "id";
	msg1.body["value"] = "6666";

	pData = myEncode.encode(&msg1,len);

	myDecode.init();

	if(!myDecode.parser(pData,len))
	{
		cout<<"parser msg1 failed!"<<endl;
	}
	else
	{
		cout<<"parser msg1 successful!"<<endl;
	}
	
	//------放入第二个消息

	msg2.head.server = 2;
	msg2.body["op"] = "get";
	msg2.body["key"] = "id";
	pData = myEncode.encode(&msg2,len);

	if(!myDecode.parser(pData,len))
	{
		cout<<"parser msg2 failed!"<<endl;
	}
	else
	{
		cout<<"parser msg2 successful!"<<endl;
	}

	//------解析两个消息
	MyProtoMsg* pMsg = NULL;

	while(!myDecode.empty())
	{
		pMsg = myDecode.front();
		printMyProtoMsg(*pMsg);
		myDecode.pop();
	}

	return 0;
}