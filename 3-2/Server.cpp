#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"

using namespace std;

//���ݲ�����󳤶�
const int max_len = (1e4)-1000; 

//�������ݰ�����󳤶� 
const int N=1e4;

#pragma comment(lib, "ws2_32.lib") 

//�����͵����� 
string sendData;

//socket 
sockaddr_in remoteAddr;
int nAddrLen;
SOCKET serSocket;

//pic=1����ǰ���ڽ���һ���ļ���������Ҫ���ļ����뱾�� 
bool pic;

//state����ǰ����״̬��0�����ֽ׶�   1�����ݷ��ͽ׶�   2�����ֽ׶Σ� 
int state;

//ȫ�����кź��ۼ�ȷ�����к� 
string ack = match("", 32), seq = match("", 32),packNum = match("", 32);
int seqnum, acknum;

//���ջ�����  
queue<package> file_que;

//�����ַ��� 
char recvData[N];

//�����жϽ������Ƿ�����һ���ļ� 
set<string> pic_set;

//���ڼ�ʱ 
clock_t start,finish;


//ά��ȫ��ack��seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}

//���ֽ׶�seqnumÿ�μ�һ 
void _rdt_send(string flag)
{
	string s="";

	seqnum += 1;
	maintain_as();

	package p("8888", "8888", flag, ack, seq,packNum , s);

	string sdata = encode( p);
	
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
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
	package p = file_que.front(); file_que.pop();
	p.print();
	assert(p.flag[FIN] == '1');
	cout <<"��һ�λ��ֳɹ�����\n";	
		
	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	string flag=match("");flag[ACK]='1';
	_rdt_send(flag);
	cout<<"�ڶ��λ��ַ��ͳɹ�\n";
	
	
	//�����λ���(FIN=1��seq=y)            s->c 
	flag[FIN]='1';
	_rdt_send(flag);
	cout<<"�����λ��ַ��ͳɹ�\n";
	
	Sleep(500);
	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	p.print();
	assert(p.flag[ACK] == '1');
	cout<<"���Ĵλ��ֽ��ճɹ�\n";
	
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	exit(0);
} 

void* receive(void* args)
{
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, N, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			package p; decode(string(recvData), p);
			
			file_que.push(p);

			//���յ������ź�,����disconnect����
			if (p.flag[FIN] == '1')
			{				
				state=2;
				disconnect();
				break;
			}
			
//			//�ۼ�ȷ�ϣ�ά��ȫ��acknum 
//			acknum = stoi(to_dec(p.seq)) + stoi(to_dec(p.len));
			
//			if(state==1)cout<<"�յ�һ����Ϣ"<<nnum++<<"\n"; 
//			if(state==1){
//				cout << "receive���յ���Ϣ��"<<p.data<<endl;//p.print();
//			}
			memset(recvData,0,sizeof(recvData))	;	
		}
	}
}

void rdt_send(string s, string seq,string packNum)
{
	s="";
	string flag = match("");
	flag[ACK] = '1';
	
	string ack=seq;
	
	package p("8888", "8888", flag, ack, seq, packNum,s);
	string sdata = encode( p);
	sendto(serSocket, sdata.c_str(),sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
}

string recv_data="";
string file_name;
void save_pic()
{
	char c;
	auto& ori = recv_data;
	string save_addr = "test\\out" + file_name;
	cout<<file_name<<endl;
	ofstream of(save_addr, ios::binary);
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
void recv_manager()
{
	while(1)
	{
		if(state==2){continue;}//�������״̬ 
		
		while(!file_que.empty())
		{
			package p=file_que.front();file_que.pop();
			if(check_lose(p))
			{
				rdt_send("",p.seq,p.packNum);
				recv_data+=p.data;
				if(pic_set.count(p.data)){
					pic=1;
					recv_data=file_name=p.data;
					start=clock(); 
				}
				if(p.flag[END]=='1')
				{
					if(pic&&!pic_set.count(p.data))
					{
						save_pic();
						finish=clock();
						cout<<"������ʱ: "<<finish-start<<"\n";
						pic=0;
					}
					else cout<<"���յ���Ϣ��"<<recv_data<<"\n";
					cout<<"recv_data.size(): "<<recv_data.size()<<"\n";
					recv_data="";
					break;
				}
				
			}
			else
			{
				rdt_send("",p.ackNum,p.packNum);
			}
		}
	}

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
	
	pic_set.insert("1.jpg");pic_set.insert("2.jpg");pic_set.insert("3.jpg");pic_set.insert("1.txt");
	return 1;
}
int main(int argc, char* argv[])
{
	if (!init())return 0;

	//������ϢҪ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//��������
	connect();

	//�����յ����ļ��� 
	recv_manager();
	
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	
	return 0;
}
