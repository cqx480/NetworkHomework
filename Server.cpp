#include <pthread.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>

#include "common.h"
#include "package.h"

using namespace std;

const double max_len = 8;//数据包最大长度 

#pragma comment(lib, "ws2_32.lib") 

string sendData;
sockaddr_in remoteAddr;
int nAddrLen;
SOCKET serSocket;
bool q;
int seqnum, acknum;
string ack=match("",32), seq=match("",32);
queue<package> file_que;


//维护全局ack和seq
void maintain_as()
{
	ack = match(to_bin(to_string(acknum)), 32);
	seq = match(to_bin(to_string(seqnum)), 32);
}

void connect()
{
	//第一次握手(SYN=1, seq=x)

	char recvData[1024];  string s; package p;
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);
			if (check_lose(p))cout << "第一次握手成功\n";
			else cout << "第一次握手失败\n";
			break;
		}
	}

	//第二次握手 SYN=1, ACK=1, seq=y, ACKnum=x+1	

	string flag = match(""), seq = match(to_bin("1"), 32);
	flag[SYN] = '1'; flag[ACK] = '1';
	int acknum = stoi(to_dec(p.seq)) + 1;
	string ack = match(to_string(acknum), 32);

	package pp("8888", "8888", flag, ack, seq, "hi");
	string sdata = encode(f, pp);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);

	//第三次握手 ACK=1，ACKnum=y+1
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);
			if (check_lose(p))cout << "第三次握手成功\n";
			else cout << "第三次握手失败\n";
			break;
		}
	}
}

//传入第一次挥手的信号 
void disconnect(package p)
{
	//第一次挥手(FIN=1，seq=x)            c->s 	

	acknum = stoi(to_dec(p.seq)) + stoi(to_dec(p.len));
	if (check_lose(p) && p.flag[FIN] != '0')cout << "第一次挥手成功\n";
	else cout << "第一次挥手失败\n";

	//先发送一个kill指令，将client的receive进程杀死 

	string flag = match(""), seq = match(to_bin(to_string(seqnum)), 32);
	flag[KILL] = '1';
	int acknum = stoi(to_dec(p.seq)) + 1;
	string ack = match(to_string(acknum), 32);

	package pkill("8888", "8888", flag, ack, seq, "kill");
	string sdata = encode(f, pkill);

	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);

	//第二次挥手(ACK=1，ACKnum=x+1)       s->c

	flag = match(""), seq = match(to_bin(to_string(seqnum)));
	flag[ACK] = '1';
	acknum = stoi(to_dec(p.seq)) + 1;
	ack = match(to_string(acknum), 32);

	package pp("8888", "8888", flag, ack, seq, "bye1");
	sdata = encode(f, pp);

	//	cout<<"sdata: "<<sdata<<endl; 
	//	package pppp;
	//	decode(sdata,pppp);cout<<pppp.flag[ACK]<<endl;

	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);


	//第三次挥手(FIN=1，seq=y)            s->c

	flag = match(""), seq = match(to_bin(to_string(seqnum)), 32);
	flag[FIN] = '1';
	acknum = stoi(to_dec(p.seq)) + 1;
	ack = match(to_bin(to_string(acknum)), 32);

	package ppp("8888", "8888", flag, ack, seq, "bye2");
	sdata = encode(f, ppp);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);

	//第四次挥手(ACK=1，ACKnum=y+1)       c->s

	char recvData[1024];
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		int ret = recvfrom(serSocket, recvData, 1024, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);
			if (check_lose(p) && p.flag[ACK] != '0')cout << "第四次挥手成功\n";
			else cout << "第四次挥手失败\n";
			break;
		}
	}

	cout << "end";
}

string recv_data;
void save_pic()
{
	char c;
	auto&ori=recv_data;
	ofstream of("_1.jpg",ios::binary);
	for(int i=0;i<ori.size();i+=8)
	{
		c=0;
		for(int j=0;j<8;++j)
		{
			int cur=ori[i+j]-'0';
			c|=(cur<<j);
		}
		of.write(&c,1);
	}
}

void* receive(void* args)
{
	nAddrLen = sizeof(remoteAddr);
	while (true)
	{
		char recvData[255];
		int ret = recvfrom(serSocket, recvData, 255, 0, (sockaddr*)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{
			recvData[ret] = 0x00;
			cout<<"111\n";

			package p; decode(string(recvData), p);

			//接收到挥手信号,进入disconnect函数(只能client向server挥手） 
			if (p.flag[FIN] == '1')
			{
				disconnect(p);
				break;
			}

			//累计确认，维护全局acknum 
			acknum = stoi(to_dec(p.seq)) + stoi(to_dec(p.len));

			printf("接受到一个连接：%s \r\n", inet_ntoa(remoteAddr.sin_addr));
			printf(recvData);
			file_que.push(p);
			cout << endl;
		}
	}
}

void rdt_send(string s, int t)
{
	string flag = match("");
	flag[ACK]='1';
	flag[ACK_GROUP]='0'+t;
	
	seqnum += s.size() / 8;
	maintain_as();

	package p("8888", "8888", flag, ack, seq, s);
	string sdata = encode(f, p);
	sendto(serSocket, sdata.c_str(), sdata.size(), 0, (sockaddr*)&remoteAddr, nAddrLen);
}

void recv_manager()
{
	int t = 0;	
	while(1)
	{
		while(file_que.empty());
		
		auto& p = file_que.front();file_que.pop();
		
		if (check_lose(p) &&p.flag[ACK_GROUP]=='0'+t)
		{
			recv_data+=p.data;
			rdt_send("", t);
			cout<<"aaaaa"<<t<<endl;
			t ^= 1;
		}
		else
		{
			rdt_send("", t ^ 1);
		}
	}
		
	
	cout<<recv_data.size()<<endl;
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
	if(!init())return 0;
	
	//三次握手
	connect();

	//接受信息要新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);

	//处理收到的文件流 
	recv_manager();

	closesocket(serSocket);
	WSACleanup();
	return 0;
}
