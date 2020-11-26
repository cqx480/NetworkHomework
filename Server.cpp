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
	//��һ������(SYN=1, seq=x)
	
	char recvData[1024];  string s; package p;
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//ĩλ��\0 
			decode(string(recvData), p);			
			if (check_lose(f, p))cout << "��һ�����ֳɹ�\n";
			else cout << "��һ������ʧ��\n";
			break;
		}
	}
	
	//�ڶ������� SYN=1, ACK=1, seq=y, ACKnum=x+1	
	
	string flag=get_16(""),seq=get_32(to_bin("555")); 
	flag[SYN]='1';flag[ACK]='1';
	int acknum=stoi(to_dec(p.seq))+1;
	string ack=get_32(to_string(acknum));
	
	package pp("8888", "8888", flag, ack, seq, "hi");
	string sdata = encode(f, pp);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);

	//���������� 
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//ĩλ��\0 
			decode(string(recvData), p);			
			if (check_lose(f, p))cout << "���������ֳɹ�\n";
			else cout << "����������ʧ��\n";
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
			printf("���ܵ�һ�����ӣ�%s \r\n", inet_ntoa(remoteAddr.sin_addr));
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

	//��������
	connect();

	//������ϢҪ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	
	
	//������Ϣ 
	while (cin >> sendData) {
		sendto(serSocket, sendData, strlen(sendData), 0, (sockaddr*)&remoteAddr, nAddrLen);
		//���Ͽ����� 
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
