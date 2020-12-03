#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

const double max_len = 8;//数据包最大长度 

SOCKET sclient;
sockaddr_in ssin;

int len, seqnum, acknum, state;
string ack = match("", 32), seq = match("", 32);
string sendData;

queue<package> file_que;


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
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			package p; decode(string(recvData), p);

			//读入文件流
			p.print();	
			file_que.push(p);
			if (state == 1)cout << "接收的消息：" << string(recvData) << "\n";
		}
	}
}
//握手阶段seqnum每次加一 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), 1024, 0, (sockaddr*)&ssin, len);
}
void rdt_send(string s, int t)
{
	string flag = match("");
	flag[ACK] = '1'; flag[ACK_GROUP] = '0' + t;

	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), 1024, 0, (sockaddr*)&ssin, len);
}

void send(string s)
{
	int t = 0;
	rdt_send(s, t);
	while (1)
	{
		while (file_que.empty());
		package p = file_que.front(); file_que.pop();

		cout << "lose: " << check_lose(p) << "\n";

		cout << "ack: " << p.isACK(t) << "\n";
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
		//检查断开连接 
		if (string(sendData) == "q")
		{
			break;
		}

		//分组发送
		int groupNum = ceil(sendData.size() / max_len);
		for (int i = 0; i < groupNum; ++i)
		{
			string groupData;
			if (groupNum == 1) groupData = sendData;
			else if (i < (groupNum - 1))groupData = sendData.substr(i * max_len, max_len);
			else groupData = sendData.substr(i * max_len);

			cout << "groupData: " << groupData << "\n";
			send(groupData);
		}
	}
}


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
