#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"

using namespace std;

const double max_len = 8;//���ݰ���󳤶� 

#pragma comment(lib, "ws2_32.lib") 

string sendData;
sockaddr_in remoteAddr;
int nAddrLen;
SOCKET serSocket;
bool q;
int seqnum, acknum;
string ack=match("",32), seq=match("",32);
queue<package> file_que;


//ά��ȫ��ack��seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}

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
			if (check_lose(p))cout << "��һ�����ֳɹ�\n";
			else cout << "��һ������ʧ��\n";
			break;
		}
	}

	//�ڶ������� SYN=1, ACK=1, seq=y, ACKnum=x+1	

	string flag = match(""), seq = match(to_bin("1"), 32);
	flag[SYN] = '1'; flag[ACK] = '1';
	int acknum = stoi(to_dec(p.seq)) + 1;
	string ack = match(to_string(acknum), 32);

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
			if (check_lose(p))cout << "���������ֳɹ�\n";
			else cout << "����������ʧ��\n";
			break;
		}
	}
}

//�����һ�λ��ֵ��ź� 
void disconnect(package p)
{
	//��һ�λ���(FIN=1��seq=x)            c->s 	

	acknum = stoi(to_dec(p.seq)) + stoi(to_dec(p.len));
	if (check_lose(p) && p.flag[FIN] != '0')cout << "��һ�λ��ֳɹ�\n";
	else cout << "��һ�λ���ʧ��\n";

	//�ȷ���һ��killָ���client��receive����ɱ�� 

	string flag = match(""), seq = match(to_bin(to_string(seqnum)), 32);
	flag[KILL] = '1';
	int acknum = stoi(to_dec(p.seq)) + 1;
	string ack = match(to_string(acknum), 32);

	package pkill("8888", "8888", flag, ack, seq, "kill");
	string sdata = encode(f, pkill);

	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);

	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c

	flag = match(""), seq = match(to_bin(to_string(seqnum)));
	flag[ACK] = '1';
	acknum = stoi(to_dec(p.seq)) + 1;
	ack = match(to_string(acknum), 32);

	package pp("8888", "8888", flag, ack, seq, "bye1");
	sdata = encode(f, pp);

	//	cout<<"sdata: "<<sdata<<endl; 
	//	package pppp;
	//	decode(sdata,pppp);cout<<pppp.flag[ACK]<<endl;

	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);


	//�����λ���(FIN=1��seq=y)            s->c

	flag = match(""), seq = match(to_bin(to_string(seqnum)), 32);
	flag[FIN] = '1';
	acknum = stoi(to_dec(p.seq)) + 1;
	ack = match(to_bin(to_string(acknum)), 32);

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
			if (check_lose(p) && p.flag[ACK] != '0')cout << "���Ĵλ��ֳɹ�\n";
			else cout << "���Ĵλ���ʧ��\n";
			break;
		}
	}

	cout << "end";
}

string recv_data;
void save_pic()
{
	char c;
	auto&ori=recv_data;
	ofstream of("_1.jpg",ios::binary);
	for(int i=0;i<ori.size();i+=8)
	{
		c=0;
		for(int j=0;j<8;++j)
		{
			int cur=ori[i+j]-'0';
			c|=(cur<<j);
		}
		of.write(&c,1);
	}
}

void* receive(void* args)
{
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		char recvData[255];
		int ret = recvfrom(serSocket, recvData, 255, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;
			cout<<"111\n";

			package p; decode(string(recvData), p);

			//���յ������ź�,����disconnect����(ֻ��client��server���֣� 
			if (p.flag[FIN] == '1')
			{
				disconnect(p);
				break;
			}

			//�ۼ�ȷ�ϣ�ά��ȫ��acknum 
			acknum = stoi(to_dec(p.seq)) + stoi(to_dec(p.len));

			printf("���ܵ�һ�����ӣ�%s \r\n", inet_ntoa(remoteAddr.sin_addr));
			printf(recvData);
			file_que.push(p);
			cout << endl;
		}
	}
}

void rdt_send(string s, int t)
{
	string flag = match("");
	flag[ACK]='1';
	flag[ACK_GROUP]='0'+t;
	
	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(f, p);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
}

void recv_manager()
{
	int t = 0;	
	while(1)
	{
		while(file_que.empty());
		
		auto& p = file_que.front();file_que.pop();
		
		if (check_lose(p) &&p.flag[ACK_GROUP]=='0'+t)
		{
			recv_data+=p.data;
			rdt_send("", t);
			cout<<"aaaaa"<<t<<endl;
			t ^= 1;
		}
		else
		{
			rdt_send("", t ^ 1);
		}
	}
		
	
	cout<<recv_data.size()<<endl;
	save_pic();

}
bool init()
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
	return 1; 	
}
int main(int argc, char* argv[])
{
	if(!init())return 0;
	
	//��������
	connect();

	//������ϢҪ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//�����յ����ļ��� 
	recv_manager();

	closesocket(serSocket);
	WSACleanup();
	return 0;
}
