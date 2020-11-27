#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

const double max_len=8;//数据包最大长度 
SOCKET sclient;
string sendData;
sockaddr_in ssin;
int len,seqnum,acknum;
string ack,seq;

fakeHead f("127.0.0.1", "127.0.0.1");

//维护全局ack和seq
void maintain_as()
{
	ack =get_32(to_bin(to_string(acknum)));
	seq =get_32(to_bin(to_string(seqnum)));
}
 
//确认重传函数 
void ret_ack(package p) 
{
	string flag = get_16("");  flag[ACK] = '1';
	//package p("8888", "8888", flag, ack, seq, "");
	string sdata = encode(f, p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);	
}


void* receive(void*args)
{
	while (true)
	{
		char recvData[255];
		int ret = recvfrom(sclient, recvData, 255, 0, (sockaddr*)&ssin, &len);
		package p;decode(recvData,p);
		if (ret > 0)
		{
			recvData[ret] = 0x00;
			
			//终止进程 
			if(p.flag[KILL]=='1')ExitThread(0);
			
			//差错检测 
			package p;decode(string(recvData),p);
			if(!check_lose(f,p))//存在丢包,tcp的做法是直接丢失 
			{
				cout<<"当前数据有损坏\n";
			}
						
			//累计确认，维护全局acknum,ack,seq 
			acknum=stoi(to_dec(p.seq))+stoi(to_dec(p.len));
			maintain_as();
//			
//			//确认重传
//			ret_ack(p);
			 
			printf(recvData);
			cout << endl;
		}
	}
}

//三次握手建立连接 
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
	
	acknum=stoi(to_dec(p.seq))+1;
	ack=get_32(to_bin(to_string(acknum)));
	
	package pp("8888", "8888", flag, ack, seq, "connect ok");
	sdata = encode(f, pp);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
	
	 
}

//四次挥手断开连接 
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

			if (check_lose(f, pack) && pack.flag[ACK] != '0') {
				cout << "第二次挥手成功\n"; continue;
			}
			if (check_lose(f, pack) && pack.flag[FIN] != '0')cout << "第三次挥手成功\n";
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
	
	//接受信息新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	
	//每次发送信息都要走一遍状态机 
	while (cin >> sendData)
	{		
		//检查断开连接 
		if(string(sendData)=="q")
		{
			break;
		}
					
		//分组发送
		int groupNum=ceil(sendData.size()/max_len); 
		for(int i=0;i<groupNum;++i)
		{
			string groupData;
			
			if(i<groupNum)groupData=sendData.substr(i*max_len,max_len);
			else groupData=sendData.substr(i*max_len);
			
			string flag = get_16("");
			seqnum += groupData.size()/8; 
			maintain_as();
	
			package p("8888", "8888", flag, ack, seq, groupData);
			string sdata = encode(f, p);
			sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);	
		}
		
	}
	
	//四次挥手 
	disconnect();
	
	closesocket(sclient);
	WSACleanup();
	return 0;
}
