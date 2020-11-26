#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

SOCKET sclient;
char sendData[1024];
sockaddr_in ssin;
int len;

fakeHead f("127.0.0.1", "127.0.0.1");

bool q;
void* send(void* args)
{
	while (cin >> sendData)
	{
		sendto(sclient, sendData, strlen(sendData), 0, (sockaddr*)&ssin, len);
//		//检查断开连接 
//		if(string(sendData)=="q")
//		{
//			q=1;
//		}
	}
}

void connect()
{
	//第一次握手 
	//cout<<"aa\n";
	string flag = get_16("");
	flag[SYN] = '1';
	
	string seq = "1",ack = get_32("");//规定起始序列号为1  
	seq = get_32(seq);
	
	package p("8888", "8888", flag, ack, seq, "hello");
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
	
	//第二次握手
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);			
			if (check_lose(f, p))cout << "第二次握手成功\n";
			else cout << "第二次握手失败\n";
			break;
		}
	} 
	
	//第三次握手(ACK=1，ACKnum=y+1)
	flag[SYN] = '0';flag[ACK]='1'; 
	
	int acknum=stoi(to_dec(p.seq))+1;
	ack=get_32(to_string(acknum));
	
	package pp("8888", "8888", flag, ack, seq, "connect ok");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
	
	 
}


void disconnect()
{
	cout<<"end";
}
int main(int argc, char* argv[])
{
	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		return 0;
	}
	sclient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	
	ssin.sin_family = AF_INET;
	ssin.sin_port = htons(8888);
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	len = sizeof(ssin);
	
	//三次握手
	connect(); 
	
	//发送信息要新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, send, NULL);
	
	while (true)
	{
		//if(q)break;
		char recvData[255];
		int ret = recvfrom(sclient, recvData, 255, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;
			printf(recvData);
			cout << endl;
		}
	}

	disconnect();
	
	closesocket(sclient);
	WSACleanup();
	return 0;
}
