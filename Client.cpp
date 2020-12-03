#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

const double max_len = 8;//���ݰ���󳤶� 

SOCKET sclient;
sockaddr_in ssin;

int len, seqnum, acknum, state;
string ack = match("", 32), seq = match("", 32);
string sendData;

queue<package> file_que;


//ά��ȫ��ack��seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}

void* receive(void* args)
{
	len = sizeof(ssin);
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			package p; decode(string(recvData), p);

			//�����ļ���
			//p.print();	
			file_que.push(p);
			if (state == 1){
				cout << "���յ���Ϣ��"<<p.data<<"\n"; 
				cout << "ACKGROUP: "<<p.flag[ACK_GROUP]<<"\n";
			}
		}
	}
}
//���ֽ׶�seqnumÿ�μ�һ 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), 1024, 0, (sockaddr*)&ssin, len);
}
//t����ack_group��sn����seq_group
void rdt_send(string s, int t,int sn)
{
	string flag = match("");
	flag[ACK] = '1'; 
	flag[ACK_GROUP] = '0' + t;
	flag[SEQ_GROUP] = '0' + sn;

	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), 1024, 0, (sockaddr*)&ssin, len);
}

void send(string s,int i)
{
	rdt_send(s, 0, i);int cnt=0;
	while (1)
	{
		while (file_que.empty());
		package p = file_que.front(); file_que.pop();

		if (check_lose(p) && p.isACK(0))
		{
			cnt++;	rdt_send(s, 1,i);
		}
		else if (cnt && check_lose(p) && p.isACK(1))
		{
			cout<<"��Ϣ���ͳɹ�����\n";
			break;
		}
		else
		{
			cout<<"��Ϣ����ʧ�ܣ��������·���\n";
			cnt=0; rdt_send(s, 0, i);
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
		
//		if(string(sendData) == "picture")
//		{
//			send(sendData);
//			ifstream in("test\\1.jpg",ios::binary);
//			char buf; 
//			while(in.read(&buf,sizeof(buf)))
//			{
//				for(int i=0;i<sizeof(buf)<<3;++i)
//				{
//					if(buf&(1<<i))send_data+='1';
//					else sendData+='0';
//				}
//			}
//		} 

		//���鷢��
		int groupNum = ceil(sendData.size() / max_len);
		string groupData;
		for (int i = 0; i < groupNum; ++i)
		{
			if (i < (groupNum - 1))groupData = sendData.substr(i * max_len, max_len);
			else groupData = sendData.substr(i * max_len);

			cout << "groupData: " << groupData << "\n";
			
			if(i < groupNum-1)send(groupData,i+1);
			else send(groupData,0);//���һ���seq_group����Ϊ0 
		}
	}
}

//�������� 
void connect()
{
	//��һ������(SYN=1, seq=x)
	string flag = match(""); flag[SYN] = '1';
	_rdt_send(flag); flag[SYN] = '0';
	cout << "��һ�����ֳɹ�����\n";

	//�ڶ������� SYN=1, ACK=1, seq=y, ACKnum=x+1
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();
	assert(p.flag[SYN] == '1' && p.flag[ACK] == '1');
	cout << "�ڶ������ֳɹ�����\n";

	//���������� ACK=1��ACKnum=y+1
	flag[ACK] = '1';
	_rdt_send(flag);
	cout << "���������ֳɹ�����\n";
	state = 1;
}

//�Ĵλ��� 
void disconnect()
{
	state = 2;
	//��һ�λ���(FIN=1��seq=x)            c->s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); flag[FIN] = '0';
	cout << "��һ�λ��ַ��ͳɹ�\n";

	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	while (file_que.empty());
	cout << "qsize: " << file_que.size() << "\n";
	package p = file_que.front(); file_que.pop(); 


	assert(p.flag[ACK] == '1');
	cout << "�ڶ��λ��ֳɹ�����\n";

	//�����λ���(FIN=1��seq=y)            s->c 
	while (file_que.empty());
	cout << "qsize: " << file_que.size() << "\n";
	p = file_que.front(); file_que.pop();

	
	assert(p.flag[FIN] == '1');
	cout << "�����λ��ֳɹ�����\n";

	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	flag[ACK] = '1'; _rdt_send(flag);
	cout << "���Ĵλ��ַ��ͳɹ�\n";
}


bool init()
{
	state = 0;//����Ҫ���õ�ǰ״̬Ϊ����ģʽ 
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
	//freopen("D:\\Desktop\\������ҵ\\��ҵ��\\NetworkHomework\\client_in.txt","w",stdout); 
	if (!init())return 0;

	//������Ϣ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	
	//�������� 
	connect();
	
	//ÿ�η�����Ϣ��Ҫ��һ��״̬�� 
	send_manager();

	//�Ĵλ��� 
	disconnect();

	closesocket(sclient);
	WSACleanup();
	return 0;
}
