#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"
//#include "ThreadSafeQueue.h"

using namespace std;

//数据部分最大长度
const int max_len = 12000; 

//整个数据包的最大长度 
const int N=14000;

//数组最大值 
const int maxn=150000;

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
string packNum = match("", 32);

//接收缓冲区  
queue<package> file_que;

//接收字符流 
char recvData[N];

//用于判断接下来是否输入一个文件 
set<string> pic_set;

//用于计时 
clock_t start,finish;

//丢包率：每mod个数据包丢一个数据包 
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
	package p = file_que.front(); file_que.pop();
	//p.print();
	assert(p.flag[FIN] == '1');
	cout <<"第一次挥手成功接收\n";	
	
	Sleep(50);	
	//第二次挥手(ACK=1，ACKnum=x+1)       s->c
	string flag=match("");
	flag[ACK]='1';
	_rdt_send(flag);
	cout<<"第二次挥手发送成功\n";
	
	Sleep(50);	
	//第三次挥手(FIN=1，seq=y)            s->c 
	flag[FIN]='1';
	_rdt_send(flag);
	cout<<"第三次挥手发送成功\n";
	
	Sleep(50);
	//第四次挥手(ACK=1，ACKnum=y+1)       c->s
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	//p.print();
	assert(p.flag[ACK] == '1');
	cout<<"第四次挥手接收成功\n";
	
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
			//cout<<"成功接收一条消息\n"; 
			
			//每mod个包就丢一个 
			int left=abs(rand())%5;
			if((cnt++)%mod==left){
				cout<<"丢弃数据包 "<< to_dec(p.packNum)<<"\n";
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
	cout<<"==============================成功发送一条消息,packNum"<<to_dec(packNum)<<"\n"; 	
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
		if(state==2){continue;}//进入挥手状态 
		
		while(state==1&&!file_que.empty())
		{
			package p=file_que.front();
			//接收到挥手信号,进入disconnect函数
			if (p.flag[FIN] == '1')
			{				
				state=2;
				//cout<<"成功断开连接！\n";
				disconnect();
				break;
			}
			file_que.pop();
			
			if(check_lose(p))
			{
				//cout<<"接收到数据包，packNum："<<to_dec(p.packNum) <<"\n";
				int pid=stoi(to_dec(p.packNum));	
//				if(pid==1110)cout<<"aaaaaaaaaaaaaaaaaa\n";	
				recv_state[pid]=1;					
				maintain_rb();
				
				//recvbase相当于nxt_seq 
				string nxt_seq=match(to_bin(to_string(recvbase)),32);
				rdt_send("",nxt_seq);
				
				//暂存收到的部分数据 
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
						//按顺序组合pic_data 
						for(int i=0;i<=pid-begin_id;++i)
						{
							if(pic_data[i]=="")cc++;
							recv_data+=pic_data[i];
						}
						
						cout<<"##################################################\n";
						cout<<"丢包率： "<<1.0/mod<<"   延时："<<delay_time<<"\n";
						cout<<"recv pic size: "<<recv_data.size()<<"\n";
						save_pic();
						finish=clock();
						cout<<"传输用时: "<<finish-start<<endl;
						double sec=(finish-start)/1000.0;
						double mb=recv_data.size()/(8.0*1024*1024);
						double v=mb/sec;
						cout<<"吞吐率："<< v<<"MB/s\n";
						pic=0;
					}
					else cout<<"接收的消息："<<recv_data<<"\n";
					cout<<"recv_data.size(): "<<recv_data.size()<<"\n";
					recv_data="";
					break;
				}
				
			}
			else
			{
				cout<<"发生差错！\n"; 
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
//	freopen("D:\\Desktop\\计网作业\\作业三\\NetworkHomework\\server.txt","w",stdout);
	if (!init())return 0;

	//接受信息要新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//三次握手
	//connect();
	state=1;

	//处理收到的文件流 
	recv_manager();
	
	system("pause");
	closesocket(serSocket);
	WSACleanup();
	
	return 0;
}
