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
			
			//差错检测 
			package p;decode(string(recvData),p);
			if(!check_lose(f,p))//存在丢包,tcp的做法是直接丢失 
			{
				cout<<"当前数据有损坏\n";
			}
			
			//终止进程 
			if(p.flag[KILL]=='1')ExitThread(0);
			
			//累计确认，维护全局acknum 
			acknum=stoi(to_dec(p.seq))+stoi(to_dec(p.len));
			
			printf(recvData);
			cout << endl;
		}
	}
}
void connect()
{
	//第一次握手 
	string flag = get_16("");
	flag[SYN] = '1';
	
	string seq = "1",ack = get_32("");//规定起始序列号为1  
	seq = get_32(seq);
	
	package p("8888", "8888", flag, ack, seq, "hello");
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
	
	//第二次握手
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			decode(string(recvData), p);			
			if (check_lose(f, p))cout << "第二次握手成功\n";
			else cout << "第二次握手失败\n";
			break;
		}
	} 
	
	//第三次握手(ACK=1，ACKnum=y+1)
	flag[SYN] = '0';flag[ACK]='1'; 
	
	int acknum=stoi(to_dec(p.seq))+1;
	ack=get_32(to_string(acknum));
	
	package pp("8888", "8888", flag, ack, seq, "connect ok");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
	
	 
}


void disconnect()
{

	//第一次挥手(FIN=1，seq=x)            c->s 
	string flag = get_16("");
	flag[FIN] = '1';
	string seq = get_32(to_bin(to_string(seqnum))), ack = get_32(to_bin(to_string(acknum)));

	package p("8888", "8888", flag, ack, seq, "bye");
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);

	//第二次挥手(ACK=1，ACKnum=x+1)       s->c
	//第三次挥手(FIN=1，seq=y)            s->c
	while (true)
	{
		char recvData[1024];
		int ret = recvfrom(sclient, recvData, 1024, 0, (sockaddr*)&ssin, &len);
		if (ret > 0)
		{
			recvData[ret] = 0x00;//末位加\0 
			package pack;
			decode(string(recvData), pack);

			//累计确认，维护全局acknum 
			acknum = stoi(to_dec(pack.seq)) + stoi(to_dec(pack.len));

//			cout << "recvData: " << recvData << endl;
//			cout << pack.flag[ACK] << endl;

			if (check_lose(f, pack) && pack.flag[ACK] != '0') {
				cout << "第二次挥手成功\n"; continue;
			}
			if (check_lose(f, pack) && pack.flag[FIN] != '0')cout << "第三次挥手成功\n";
			//else cout << "第三次挥手失败\n";
			break;
		}
	}

	//第四次挥手(ACK=1，ACKnum=y+1)       c->s
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
	
	//三次握手
	connect(); 
	
	//接受信息要新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	
	//每次发送信息都要走一遍状态机 
	while (cin >> sendData)
	{				
		//正常发送 
		string flag = get_16("");
		seqnum += sendData.size(); 
		string seq = get_32(to_bin(to_string(seqnum)));
		string ack = get_32(to_bin(to_string(acknum)));
	
		package p("8888", "8888", flag, ack, seq, sendData);
		string sdata = encode(f, p);
		sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);		
		
		//检查断开连接,同时把recv线程关掉 
		if(string(sendData)=="q")
		{
			WaitForSingleObject(thread, INFINITE);
			CloseHandle(thread);
			break;
		}
	}
	
	//四次挥手 
	disconnect();
	
	closesocket(sclient);
	WSACleanup();
	return 0;
}
