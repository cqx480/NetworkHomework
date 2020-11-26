#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"

using namespace std;

#pragma comment(lib, "ws2_32.lib") 

char  sendData[1024];
sockaddr_in remoteAddr;
int nAddrLen;
SOCKET serSocket;
fakeHead f("127.0.0.1", "127.0.0.1");

void connect()
{
	//第一次握手(SYN=1, seq=x)
	
	char recvData[1024];  string s; package p;
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);			
			if (check_lose(f, p))cout << "第一次握手成功\n";
			else cout << "第一次握手失败\n";
			break;
		}
	}
	
	//第二次握手 SYN=1, ACK=1, seq=y, ACKnum=x+1	
	
	string flag=get_16(""),seq=get_32(to_bin("555")); 
	flag[SYN]='1';flag[ACK]='1';
	int acknum=stoi(to_dec(p.seq))+1;
	string ack=get_32(to_string(acknum));
	
	package pp("8888", "8888", flag, ack, seq, "hi");
	string sdata = encode(f, pp);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);

	//第三次握手 
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);			
			if (check_lose(f, p))cout << "第三次握手成功\n";
			else cout << "第三次握手失败\n";
			break;
		}
	}
}

void disconnect()
{
	cout<<"end"; 
}
bool q;

void* receive(void*args)
{
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		char recvData[255];
		int ret = recvfrom(serSocket, recvData, 255, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;
			printf("接受到一个连接：%s \r\n", inet_ntoa(remoteAddr.sin_addr));
			printf(recvData);
			cout << endl;
		}
	}
}
int main(int argc, char* argv[])
{
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 0;
	}

	serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serSocket == INVALID_SOCKET)
	{
		printf("socket error !");
		return 0;
	}

	sockaddr_in serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(8888);
	serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(serSocket, (sockaddr*)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
	{
		printf("bind error !");
		closesocket(serSocket);
		return 0;
	}

	//三次握手
	connect();

	//接受信息要新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	
	
	//发送信息 
	while (cin >> sendData) {
		sendto(serSocket, sendData, strlen(sendData), 0, (sockaddr*)&remoteAddr, nAddrLen);
		//检查断开连接 
		if(string(sendData)=="q")
		{
			break;
		}
	}
	
	
	disconnect();
	
	closesocket(serSocket);
	WSACleanup();
	return 0;
}
