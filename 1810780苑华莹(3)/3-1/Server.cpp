#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"

using namespace std;

//数据部分最大长度
const int max_len = (1e4)-1000; 

//整个数据包的最大长度 
const int N=1e4;

#pragma comment(lib, "ws2_32.lib") 

//待发送的数据 
string sendData;

//socket 
sockaddr_in remoteAddr;
int nAddrLen;
SOCKET serSocket;

//pic=1代表当前正在接收一个文件，接下来要将文件存入本地 
bool pic;

//state代表当前所处状态（0：握手阶段   1：数据发送阶段   2：挥手阶段） 
int state;

//全局序列号和累计确认序列号 
string ack = match("", 32), seq = match("", 32);
int seqnum, acknum;

//接收缓冲区  
queue<package> file_que;

//接收字符流 
char recvData[N];

//用于判断接下来是否输入一个文件 
set<string> pic_set;

//用于计时 
clock_t start,finish;


//维护全局ack和seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}

//握手阶段seqnum每次加一 
void _rdt_send(string flag)
{
	string s="";

	seqnum += 1;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	//p.print();
	string sdata = encode( p);
	
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
}

void connect()
{
	//第一次握手(SYN=1, seq=x)
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();
	string s=encode(p);//cout<<"处理的消息:"<<s<<endl;
	
	assert(p.flag[SYN] == '1');
	cout << "第一次握手成功接收\n";

	//第二次握手 SYN=1, ACK=1, seq=y, ACKnum=x+1
	string flag = match(""); flag[SYN] = '1'; flag[ACK] = '1';
	_rdt_send(flag);
	cout << "第二次握手成功发送\n";

	//第三次握手 ACK=1，ACKnum=y+1
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	assert(p.flag[ACK] == '1');
	cout << "第三次握手成功接收\n";
	state=1;
}

void disconnect()
{
	state=2;
	//第一次挥手(FIN=1，seq=x)            c->s
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();//file_que.pop();
	assert(p.flag[FIN] == '1');
	cout <<"第一次挥手成功接收\n";	
		
	//第二次挥手(ACK=1，ACKnum=x+1)       s->c
	string flag=match("");flag[ACK]='1';
	_rdt_send(flag);flag[ACK]='0';
	cout<<"第二次挥手发送成功\n";
	
	
	//第三次挥手(FIN=1，seq=y)            s->c 
	flag[FIN]='1';
	_rdt_send(flag);flag[FIN]='0';
	cout<<"第三次挥手发送成功\n";
	
	//第四次挥手(ACK=1，ACKnum=y+1)       c->s
	while (file_que.empty());
	p = file_que.front(); file_que.pop();//file_que.pop();
	assert(p.flag[ACK] == '1');
	cout<<"第四次挥手接收成功\n";
	
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	
} 
//int nnum; 
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

			//接收到挥手信号,进入disconnect函数
			if (p.flag[FIN] == '1')
			{				
				state=2;
			}
			memset(recvData,0,sizeof(recvData))	;	
		}
	}
}

void rdt_send(string s, int t)
{
	s="状态";
	char tmp=(t+'0');
	string ttmp="确认成功 ";
	s+=tmp;s+=ttmp;
	string flag = match("");
	flag[ACK] = '1';
	flag[ACK_GROUP] = '0' + t;

	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
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
	int t=0;
	while(1)
	{
		if(state==2){disconnect();break;}//进入挥手状态 
		
		while(state==1&&!file_que.empty())
		{
			package p=file_que.front();
			//接收到挥手信号,进入disconnect函数
			if (p.flag[FIN] == '1')
			{				
				state=2;
				disconnect();
				break;
			}
			file_que.pop();
			
			if(check_lose(p)&&p.flag[ACK_GROUP]=='0'+t)
			{
				recv_data+=p.data;
				rdt_send("",t);
				t^=1;
				if(pic_set.count(p.data)){
					pic=1;file_name=p.data;start=clock(); 
				}
				if(p.flag[END]=='1')
				{
					//assert(pic);
					if(pic&&!pic_set.count(p.data))
					{
						save_pic();
						finish=clock();
						cout<<"传输用时: "<<finish-start<<endl;
						double sec=(finish-start)/1000.0;
						double mb=recv_data.size()/(8.0*1024*1024);
						double v=mb/sec;
						cout<<"吞吐率："<< v<<"MB/s\n";
						
						pic=0;
					}
					else cout<<"接收的消息："<<recv_data<<endl;
					cout<<"recv_data.size(): "<<recv_data.size()<<"\n";
					recv_data="";
					break;
				}
				
			}
			else
			{
				cout<<"发生差错！"<<endl; 
				rdt_send("",p.ackgroup());
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

	//接受信息要新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//三次握手
	connect();

	//处理收到的文件流 
	recv_manager();
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	
	return 0;
}
