#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

//数据部分最大长度
const int max_len = (1e4)-1000; 

//整个数据包的最大长度 
const int N=1e4;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state代表当前所处状态（0：握手阶段   1：数据发送阶段   2：挥手阶段） 
int state;

//全局序列号和累计确认序列号 
string ack = match("", 32), seq = match("", 32);
int seqnum, acknum; 

//待发送的字符串 
string sendData;

//输入流 
char recvData[N];

//目的端口 
string SourcePort;

//接收缓冲区 
queue<package> file_que;

//奇偶序列号 
int t;

//文件名称 
set<string>pic_set;

//用于计时 
clock_t start,finish;

//维护全局ack和seq
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
		int ret = recvfrom(sclient, recvData, N, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			package p; decode(string(recvData), p);	
			file_que.push(p);
			memset(recvData,0,sizeof(recvData));
		}
	}
}
//握手阶段seqnum每次加一 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;
	maintain_as();

	package p(SourcePort, SourcePort, flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}
//t代表ack_group，
void rdt_send(string s, int t,bool end)
{
	string flag = match("");
	flag[ACK] = '1'; 
	flag[ACK_GROUP] = '0' + t;
	
	//最后一组要加标志位 
	if(end)flag[END]='1';

	seqnum += s.size() / 8;
	maintain_as();

	package p(SourcePort, SourcePort, flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

void send()
{
	//分组发送
	int groupNum = (sendData.size()+max_len-1)/max_len;
	string groupData;
	for (int i = 0; i < groupNum; ++i)
	{
		if (i < (groupNum - 1))groupData = sendData.substr(i * max_len, max_len);
		else groupData = sendData.substr(i * max_len);
		bool end=(i==groupNum-1);
		
		
		//下面开始分奇偶发送 
		rdt_send(groupData,t,end);
		start=clock();		
		while(1)
		{
			while(file_que.empty());
			package p=file_que.front();file_que.pop();
			//cout<<t<<" "<<p.isACK(t)<<"\n";
			finish=clock();
			if(check_lose(p)&&p.isACK(t))
			{
				t^=1;
				break;
			}
			else if((finish-start)>10||!p.isACK(t)||!check_lose(p))
			{
				rdt_send(sendData,t,end);
			}
		}
		
//		cout << "groupData: " << groupData << "\n";
//		cout<<"第"<<i+1<<"组成功发送\n"; 
	}
} 
void get_pic()
{
	send();
	
	//读入的文件地址 
	string file_addr="test\\"+sendData;
	
	ifstream in(file_addr,ios::binary);
	
	sendData="";
	
	char buf; 
	while(in.read(&buf,sizeof(buf)))
	{
		for(int i=0;i<sizeof(buf)<<3;++i)
		{
			if(buf&(1<<i))sendData+='1';
			else sendData+='0';
		}
	}
	cout<<"pic size: "<<sendData.size()<<endl; 	
}

void send_manager()
{
	
	while (cin >> sendData)
	{
		//检查断开连接 
		if (sendData == "q")
		{
			state=2;
			break;
		}
		
		if(pic_set.count(sendData))
		{
			get_pic();
		} 
		
		send();
	
	}
}

//三次握手 
void connect()
{
	//第一次握手(SYN=1, seq=x)
	string flag = match(""); flag[SYN] = '1';
	_rdt_send(flag); flag[SYN] = '0';
	cout << "第一次握手成功发送\n";

	//第二次握手 SYN=1, ACK=1, seq=y, ACKnum=x+1
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();
	assert(p.flag[SYN] == '1' && p.flag[ACK] == '1');
	cout << "第二次握手成功接收\n";

	//第三次握手 ACK=1，ACKnum=y+1
	flag[ACK] = '1';
	_rdt_send(flag);
	cout << "第三次握手成功发送\n";
	state = 1;
}

//四次挥手 
void disconnect()
{
	state = 2;
	//第一次挥手(FIN=1，seq=x)            c->s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); flag[FIN] = '0';
	cout << "第一次挥手发送成功\n";

	//第二次挥手(ACK=1，ACKnum=x+1)       s->c
	while (file_que.empty());
	cout << "qsize: " << file_que.size() << "\n";
	package p = file_que.front(); file_que.pop(); 
	assert(p.flag[ACK] == '1');
	cout << "第二次挥手成功接收\n";

	//第三次挥手(FIN=1，seq=y)            s->c 
	while (file_que.empty());
	cout << "qsize: " << file_que.size() << "\n";
	p = file_que.front(); file_que.pop();

	
	assert(p.flag[FIN] == '1');
	cout << "第三次挥手成功接收\n";

	//第四次挥手(ACK=1，ACKnum=y+1)       c->s
	flag[ACK] = '1'; _rdt_send(flag);
	cout << "第四次挥手发送成功\n";
}


bool init()
{
	state = 0;//首先要设置当前状态为握手模式 
	pic_set.insert("1.jpg");pic_set.insert("2.jpg");pic_set.insert("3.jpg");pic_set.insert("1.txt");
	
	
	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		return 0;
	}
	sclient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	
	ssin.sin_family = AF_INET;
	ssin.sin_port = htons(stoi(SourcePort));
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	len = sizeof(ssin);

	return 1;
}
int main(int argc, char* argv[])
{
	cout<<"please input the source port: ";
	cin>>SourcePort;
	
	//freopen("D:\\Desktop\\计网作业\\作业三\\NetworkHomework\\client_in.txt","w",stdout); 
	if (!init())return 0;

	//接受信息新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	

	 
	//三次握手 
	connect();
	
	//每次发送信息都要走一遍状态机 
	send_manager();

	//四次挥手 
	disconnect();

	closesocket(sclient);
	WSACleanup();
	return 0;
}
