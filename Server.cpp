#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"

using namespace std;

#pragma comment(lib, "ws2_32.lib") 

string sendData;
sockaddr_in remoteAddr;
int nAddrLen;
SOCKET serSocket;
fakeHead f("127.0.0.1", "127.0.0.1");
bool q;
int seqnum,acknum; 

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

	//���������� ACK=1��ACKnum=y+1
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

//�����һ�λ��ֵ��ź� 
void disconnect(package p)
{	
	//��һ�λ���(FIN=1��seq=x)            c->s 	
		
	acknum=stoi(to_dec(p.seq))+stoi(to_dec(p.len));	
	if (check_lose(f, p)&&p.flag[FIN]!='0')cout << "��һ�λ��ֳɹ�\n";
	else cout << "��һ�λ���ʧ��\n";	
	
	//�ȷ���һ��killָ���client��receive����ɱ�� 
	
	string flag=get_16(""),seq=get_32(to_bin(to_string(seqnum))); 
	flag[KILL]='1';
	int acknum=stoi(to_dec(p.seq))+1;
	string ack=get_32(to_string(acknum));
	
	package pkill("8888", "8888", flag, ack, seq, "kill");
	string sdata = encode(f, pkill);
	
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);	
	 
	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
		
	flag=get_16(""),seq=get_32(to_bin(to_string(seqnum))); 
	flag[ACK]='1';
	acknum=stoi(to_dec(p.seq))+1;
	ack=get_32(to_string(acknum));
	
	package pp("8888", "8888", flag, ack, seq, "bye1");
	sdata = encode(f, pp);
	
//	cout<<"sdata: "<<sdata<<endl; 
//	package pppp;
//	decode(sdata,pppp);cout<<pppp.flag[ACK]<<endl;
	
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
	
	
	//�����λ���(FIN=1��seq=y)            s->c
	
	flag=get_16(""),seq=get_32(to_bin(to_string(seqnum))); 
	flag[FIN]='1';
	acknum=stoi(to_dec(p.seq))+1;
	ack=get_32(to_bin(to_string(acknum)));
	
	package ppp("8888", "8888", flag, ack, seq, "bye2");
	sdata = encode(f, ppp);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
	
	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	
	char recvData[1024]; 
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//ĩλ��\0 
			decode(string(recvData), p);			
			if (check_lose(f, p)&&p.flag[ACK]!='0')cout << "���Ĵλ��ֳɹ�\n";
			else cout << "���Ĵλ���ʧ��\n";
			break;
		}
	}
	
	cout<<"end"; 
}


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
			
			//����� 
			package p;decode(string(recvData),p);
			if(!check_lose(f,p))//���ڶ���,tcp��������ֱ�Ӷ�ʧ 
			{
				cout<<"��ǰ��������\n";
			}
			
			//���յ������ź�,����disconnect����(ֻ��client��server���֣� 
			if(p.flag[FIN]=='1')
			{
				disconnect(p); 
				break; 
			}
			
			//�ۼ�ȷ�ϣ�ά��ȫ��acknum 
			acknum=stoi(to_dec(p.seq))+stoi(to_dec(p.len));
			
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
		
		//�������� 
		string flag = get_16("");
		seqnum += sendData.size(); 
		string seq = get_32(to_bin(to_string(seqnum))); 
		string ack = get_32(to_bin(to_string(acknum)));
	
		package p("8888", "8888", flag, ack, seq, sendData);
		string sdata = encode(f, p);
		sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);	
		
		//���Ͽ����� 
		if(string(sendData)=="q"||q)
		{
			break;
		}
	}
	
	
	
	closesocket(serSocket);
	WSACleanup();
	return 0;
}
