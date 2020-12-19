#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"
//#include "ThreadSafeQueue.h"

using namespace std;

//���ݲ�����󳤶�
const int max_len = 12000; 

//�������ݰ�����󳤶� 
const int N=14000;

//�������ֵ 
const int maxn=150000;

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
string packNum = match("", 32);

//���ջ�����  
queue<package> file_que;

//�����ַ��� 
char recvData[N];

//�����жϽ������Ƿ�����һ���ļ� 
set<string> pic_set;

//���ڼ�ʱ 
clock_t start,finish;

//�����ʣ�ÿmod�����ݰ���һ�����ݰ� 
int mod=40;

int recvbase; 

int recv_state[maxn];

void _rdt_send(string flag)
{
	string s="";

	package p("8888", "8888", flag, packNum , s);

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
	//p.print();
	assert(p.flag[FIN] == '1');
	cout <<"��һ�λ��ֳɹ�����\n";	
	
	Sleep(50);	
	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	string flag=match("");
	flag[ACK]='1';
	_rdt_send(flag);
	cout<<"�ڶ��λ��ַ��ͳɹ�\n";
	
	Sleep(50);	
	//�����λ���(FIN=1��seq=y)            s->c 
	flag[FIN]='1';
	_rdt_send(flag);
	cout<<"�����λ��ַ��ͳɹ�\n";
	
	Sleep(50);
	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	//p.print();
	assert(p.flag[ACK] == '1');
	cout<<"���Ĵλ��ֽ��ճɹ�\n";
	
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	exit(0);
} 

int cnt;
void* receive(void* args)
{
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, N, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			package p; decode(string(recvData), p);
			//cout<<"�ɹ�����һ����Ϣ\n"; 
			
			//ÿmod�����Ͷ�һ�� 
			int left=abs(rand())%5;
			if((cnt++)%mod==left){
				cout<<"�������ݰ� "<< to_dec(p.packNum)<<"\n";
				continue;	
			}	
			cout<<"recvbase: "<<recvbase<<"\n";	
			file_que.push(p);
			memset(recvData,0,sizeof(recvData))	;	
		}
	}
}

void rdt_send(string s, string packNum)
{
	cout<<"==============================�ɹ�����һ����Ϣ,packNum"<<to_dec(packNum)<<"\n"; 	
	s="";
	string flag = match("");
	flag[ACK] = '1';
		
	package p("8888", "8888", flag, packNum,s);
	string sdata = encode( p);
	sendto(serSocket, sdata.c_str(),sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
}

string recv_data="";
string file_name;
void maintain_rb()
{
	while(recv_state[recvbase])recvbase++;
}

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
int begin_id;
int delay_time;
string pic_data[maxn];
void recv_manager()
{
	while(1)
	{		
		if(state==2){continue;}//�������״̬ 
		
		while(state==1&&!file_que.empty())
		{
			package p=file_que.front();
			//���յ������ź�,����disconnect����
			if (p.flag[FIN] == '1')
			{				
				state=2;
				//cout<<"�ɹ��Ͽ����ӣ�\n";
				disconnect();
				break;
			}
			file_que.pop();
			
			if(check_lose(p))
			{
				//cout<<"���յ����ݰ���packNum��"<<to_dec(p.packNum) <<"\n";
				int pid=stoi(to_dec(p.packNum));	
//				if(pid==1110)cout<<"aaaaaaaaaaaaaaaaaa\n";	
				recv_state[pid]=1;					
				maintain_rb();
				
				//recvbase�൱��nxt_seq 
				string nxt_seq=match(to_bin(to_string(recvbase)),32);
				rdt_send("",nxt_seq);
				
				//�ݴ��յ��Ĳ������� 
				pic_data[pid-begin_id]=p.data;
				int bb=0;
				if(p.data=="")bb++;
				
				if(pic_set.count(p.data)){
					pic=1;
					recv_data=file_name=p.data;
					begin_id=pid+1;
					start=clock(); 
				}
				if(p.flag[END]=='1')
				{
					if(pic&&!pic_set.count(p.data))
					{
						int cc=0;
						//��˳�����pic_data 
						for(int i=0;i<=pid-begin_id;++i)
						{
							if(pic_data[i]=="")cc++;
							recv_data+=pic_data[i];
						}
						
						cout<<"##################################################\n";
						cout<<"�����ʣ� "<<1.0/mod<<"   ��ʱ��"<<delay_time<<"\n";
						cout<<"recv pic size: "<<recv_data.size()<<"\n";
						save_pic();
						finish=clock();
						cout<<"������ʱ: "<<finish-start<<endl;
						double sec=(finish-start)/1000.0;
						double mb=recv_data.size()/(8.0*1024*1024);
						double v=mb/sec;
						cout<<"�����ʣ�"<< v<<"MB/s\n";
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
				cout<<"�������\n"; 
				rdt_send("",p.packNum);
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
	serAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.2");//inet_addr("10.134.146.124");//INADDR_ANY;//inet_addr("127.0.0.1");

	if (bind(serSocket, (sockaddr*)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
	{
		printf("bind error !");
		closesocket(serSocket);
		return 0;
	}
	
	pic_set.insert("1.jpg");pic_set.insert("2.jpg");pic_set.insert("3.jpg");pic_set.insert("1.txt");
	pic_set.insert("small.png");pic_set.insert("mid.png");
	return 1;
}
int main(int argc, char* argv[])
{
//	freopen("D:\\Desktop\\������ҵ\\��ҵ��\\NetworkHomework\\server.txt","w",stdout);
	if (!init())return 0;

	//������ϢҪ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//��������
	//connect();
	state=1;

	//�����յ����ļ��� 
	recv_manager();
	
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	
	return 0;
}
