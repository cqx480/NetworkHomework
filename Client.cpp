#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

const double max_len = 8;//���ݰ���󳤶� 
SOCKET sclient;
string sendData;
sockaddr_in ssin;
int len, seqnum, acknum;
string ack, seq;
queue<package> file_que;


//ά��ȫ��ack��seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}

void* receive(void* args)
{
	while (true)
	{
		char recvData[255];
		int ret = recvfrom(sclient, recvData, 255, 0, (sockaddr*)&ssin, &len);
		package p; decode(recvData, p);
		if (ret > 0)
		{
			recvData[ret] = 0x00;

			//��ֹ���� 
			if (p.flag[KILL] == '1')ExitThread(0);
			//�����ļ���	
			file_que.push(p);
		}
	}
}

void rdt_send(string s, int t)
{
	string flag = match("");
	flag[ACK_GROUP]='0'+t;
	
	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

void send(string s)
{
	int t = 0;
	rdt_send(s, t);
	while (1)
	{
		while (file_que.empty());
		package p = file_que.front(); file_que.pop();
		
     	cout<<"lose: "<<check_lose(p)<<endl;
     	
     	cout<<"ack: "<<p.isACK(t)<<endl;
		if (check_lose(p) && p.isACK(t))
		{
			t ^= 1;
			break;
		}
		else
		{
			rdt_send(s, t);
		}
	}
}
void send_manager()
{
	while (cin >> sendData)
	{
		//���Ͽ����� 
		if (string(sendData) == "q")
		{
			break;
		}

		//���鷢��
		int groupNum = ceil(sendData.size() / max_len);
		for (int i = 0; i < groupNum; ++i)
		{
			string groupData;
			if(groupNum==1) groupData=sendData;
			else if (i < (groupNum-1))groupData = sendData.substr(i * max_len, max_len);
			else groupData = sendData.substr(i * max_len);
			
			cout<<"groupData: "<<groupData<<endl;
			send(groupData);
		}
	}
}
//�������ֽ������� 
void connect()
{
	//��һ������ 
	string flag = match("");
	flag[SYN] = '1';

	string seq = "1", ack = match("", 32);//�涨��ʼ���к�Ϊ1  
	seq = match(seq, 32);

	package p("8888", "8888", flag, ack, seq, "hello");
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);

	//�ڶ�������
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//ĩλ��\0 
			decode(string(recvData), p);
			if (check_lose(p))cout << "�ڶ������ֳɹ�\n";
			else cout << "�ڶ�������ʧ��\n";
			break;
		}
	}

	//����������(ACK=1��ACKnum=y+1)
	flag[SYN] = '0'; flag[ACK] = '1';

	acknum = stoi(to_dec(p.seq)) + 1;
	ack = match(to_bin(to_string(acknum)), 32);

	package pp("8888", "8888", flag, ack, seq, "connect ok");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);


}

//�Ĵλ��ֶϿ����� 
void disconnect()
{

	//��һ�λ���(FIN=1��seq=x)            c->s 
	string flag = match("");
	flag[FIN] = '1';
	string seq = match(to_bin(to_string(seqnum)), 32), ack = match(to_bin(to_string(acknum)), 32);

	package p("8888", "8888", flag, ack, seq, "bye");
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);

	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	//�����λ���(FIN=1��seq=y)            s->c
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//ĩλ��\0 
			package pack;
			decode(string(recvData), pack);

			//�ۼ�ȷ�ϣ�ά��ȫ��acknum 
			acknum = stoi(to_dec(pack.seq)) + stoi(to_dec(pack.len));

			if (check_lose(pack) && pack.flag[ACK] != '0') {
				cout << "�ڶ��λ��ֳɹ�\n"; continue;
			}
			if (check_lose(pack) && pack.flag[FIN] != '0')cout << "�����λ��ֳɹ�\n";
			break;
		}
	}

	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	flag = match("");	flag[ACK] = '1';
	seq = match(to_bin(to_string(seqnum)), 32), ack = match(to_bin(to_string(acknum)), 32);

	package pp("8888", "8888", flag, ack, seq, "bye");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);

	cout << "end";
}

bool init()
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

	return 1;		
}
int main(int argc, char* argv[])
{
	if(!init())return 0;
		
	//��������
	connect();
	
	//������Ϣ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//ÿ�η�����Ϣ��Ҫ��һ��״̬�� 
	send_manager();

	//�Ĵλ��� 
	disconnect();

	closesocket(sclient);
	WSACleanup();
	return 0;
}
