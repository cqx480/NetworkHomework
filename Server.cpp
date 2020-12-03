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
int seqnum, acknum,state;
string ack = match("", 32), seq = match("", 32);
queue<package> file_que;


//ά��ȫ��ack��seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}


string recv_data;
void save_pic()
{
	char c;
	auto& ori = recv_data;
	ofstream of("_1.jpg", ios::binary);
	for (int i = 0; i < ori.size(); i += 8)
	{
		c = 0;
		for (int j = 0; j < 8; ++j)
		{
			int cur = ori[i + j] - '0';
			c |= (cur << j);
		}
		of.write(&c, 1);
	}
}

//���ֽ׶�seqnumÿ�μ�һ 
void _rdt_send(string flag)
{
	//cout<<flag[FIN]<<endl;
	string s="";

	seqnum += 1;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	//p.print();
	string sdata = encode( p);
	
	sendto(serSocket, sdata.c_str(), 1024, 0, (sockaddr*)&remoteAddr, nAddrLen);
}

void connect()
{
	//��һ������(SYN=1, seq=x)
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();
	string s=encode(p);//cout<<"�������Ϣ:"<<s<<endl;
	
	assert(p.flag[SYN] == '1');
	cout << "��һ�����ֳɹ�����\n";

	//�ڶ������� SYN=1, ACK=1, seq=y, ACKnum=x+1
	string flag = match(""); flag[SYN] = '1'; flag[ACK] = '1';
	_rdt_send(flag);
	cout << "�ڶ������ֳɹ�����\n";

	//���������� ACK=1��ACKnum=y+1
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	assert(p.flag[ACK] == '1');
	cout << "���������ֳɹ�����\n";
	state=1;
}


void disconnect()
{
	state=2;
	//��һ�λ���(FIN=1��seq=x)            c->s
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();//file_que.pop();
	assert(p.flag[FIN] == '1');
	cout <<"��һ�λ��ֳɹ�����\n";	
		
	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	string flag=match("");flag[ACK]='1';
	_rdt_send(flag);flag[ACK]='0';
	cout<<"�ڶ��λ��ַ��ͳɹ�\n";
	
	
	//�����λ���(FIN=1��seq=y)            s->c 
	flag[FIN]='1';
	_rdt_send(flag);flag[FIN]='0';
	cout<<"�����λ��ַ��ͳɹ�\n";
	
	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	while (file_que.empty());
	p = file_que.front(); file_que.pop();//file_que.pop();
	assert(p.flag[ACK] == '1');
	cout<<"���Ĵλ��ֽ��ճɹ�\n";
	
	Sleep(1000);
	closesocket(serSocket);
	WSACleanup();
	exit(0);
} 

void* receive(void* args)
{
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			package p; decode(string(recvData), p);
			
			file_que.push(p);

			//���յ������ź�,����disconnect����
			if (p.flag[FIN] == '1')
			{				
				state=2;
			}
			//�ۼ�ȷ�ϣ�ά��ȫ��acknum 
			acknum = stoi(to_dec(p.seq)) + stoi(to_dec(p.len));

			if(state==1)cout << "���յ���Ϣ��"<<string(recvData)<<"\n";
			
		}
	}
}

void rdt_send(string s, int t)
{
	string flag = match("");
	flag[ACK] = '1';
	flag[ACK_GROUP] = '0' + t;

	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode( p);
	sendto(serSocket, sdata.c_str(),1024, 0, (sockaddr*)&remoteAddr, nAddrLen);
}

void recv_manager()
{
	int t = 0;
	while (1)
	{
		if(state=2){disconnect();break;};//�������״̬ 
		while (file_que.empty());

		auto& p = file_que.front(); file_que.pop();

		if (check_lose(p) && p.flag[ACK_GROUP] == '0' + t)
		{
			recv_data += p.data;
			rdt_send("", t);
			cout << "aaaaa" << t << endl;
			t ^= 1;
		}
		else
		{
			rdt_send("", t ^ 1);
		}
	}

	cout << recv_data.size() << endl;
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
	//freopen("D:\\Desktop\\������ҵ\\��ҵ��\\NetworkHomework\\server_out.txt","w",stdout); 

	if (!init())return 0;

	//������ϢҪ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//��������
	connect();


	//�����յ����ļ��� 
	recv_manager();

	closesocket(serSocket);
	WSACleanup();
	return 0;
}
