#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

SOCKET sclient;
string sendData;
sockaddr_in ssin;
int len,seqnum,acknum;

fakeHead f("127.0.0.1", "127.0.0.1");


void* receive(void*args)
{
	while (true)
	{
		char recvData[255];
		int ret = recvfrom(sclient, recvData, 255, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;
			
			//����� 
			package p;decode(string(recvData),p);
			if(!check_lose(f,p))//���ڶ���,tcp��������ֱ�Ӷ�ʧ 
			{
				cout<<"��ǰ��������\n";
			}
			
			//��ֹ���� 
			if(p.flag[KILL]=='1')ExitThread(0);
			
			//�ۼ�ȷ�ϣ�ά��ȫ��acknum 
			acknum=stoi(to_dec(p.seq))+stoi(to_dec(p.len));
			
			printf(recvData);
			cout << endl;
		}
	}
}
void connect()
{
	//��һ������ 
	string flag = get_16("");
	flag[SYN] = '1';
	
	string seq = "1",ack = get_32("");//�涨��ʼ���к�Ϊ1  
	seq = get_32(seq);
	
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
			if (check_lose(f, p))cout << "�ڶ������ֳɹ�\n";
			else cout << "�ڶ�������ʧ��\n";
			break;
		}
	} 
	
	//����������(ACK=1��ACKnum=y+1)
	flag[SYN] = '0';flag[ACK]='1'; 
	
	int acknum=stoi(to_dec(p.seq))+1;
	ack=get_32(to_string(acknum));
	
	package pp("8888", "8888", flag, ack, seq, "connect ok");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
	
	 
}


void disconnect()
{

	//��һ�λ���(FIN=1��seq=x)            c->s 
	string flag = get_16("");
	flag[FIN] = '1';
	string seq = get_32(to_bin(to_string(seqnum))), ack = get_32(to_bin(to_string(acknum)));

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

//			cout << "recvData: " << recvData << endl;
//			cout << pack.flag[ACK] << endl;

			if (check_lose(f, pack) && pack.flag[ACK] != '0') {
				cout << "�ڶ��λ��ֳɹ�\n"; continue;
			}
			if (check_lose(f, pack) && pack.flag[FIN] != '0')cout << "�����λ��ֳɹ�\n";
			//else cout << "�����λ���ʧ��\n";
			break;
		}
	}

	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	flag = get_16("");	flag[ACK] = '1';
	seq = get_32(to_bin(to_string(seqnum))), ack = get_32(to_bin(to_string(acknum)));

	package pp("8888", "8888", flag, ack, seq, "bye");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);

	cout << "end";
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
	
	//��������
	connect(); 
	
	//������ϢҪ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	
	//ÿ�η�����Ϣ��Ҫ��һ��״̬�� 
	while (cin >> sendData)
	{				
		//�������� 
		string flag = get_16("");
		seqnum += sendData.size(); 
		string seq = get_32(to_bin(to_string(seqnum)));
		string ack = get_32(to_bin(to_string(acknum)));
	
		package p("8888", "8888", flag, ack, seq, sendData);
		string sdata = encode(f, p);
		sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);		
		
		//���Ͽ�����,ͬʱ��recv�̹߳ص� 
		if(string(sendData)=="q")
		{
			WaitForSingleObject(thread, INFINITE);
			CloseHandle(thread);
			break;
		}
	}
	
	//�Ĵλ��� 
	disconnect();
	
	closesocket(sclient);
	WSACleanup();
	return 0;
}
