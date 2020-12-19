#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"
#include "unrecv.cpp"

//#include "ThreadSafeQueue.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

#define MSS max_len

//数据部分最大长度
const int max_len = 12000; 

//sendbase最大值 
const int maxn=150000;
 
//整个数据包的最大长度 
const int N=14000;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state代表当前所处状态（0：握手阶段   1：数据发送阶段   2：挥手阶段） 
int state;

string packNum = match("", 32);

//ack_state[i]代表packNum=1的数据包是否成功接收 
int ack_state[maxn];
int valid[maxn];


//待发送的字符串 
string sendData;

//输入流 
char recvData[N];

//源端口，目的端口 
string SourcePort="8888",DesPort;



//文件名称 
set<string>pic_set;


int groupNum;

void timeout_handler();



//已经收到的连续ack的最大值 
int sendbase;

//初始窗口大小为1
int win_size=1;

//重复ack个数 
int dupACKcount;

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

//采用慢启动拥塞控制算法，初始win_size=1
//窗口大小的阈值设置为3 ,cur代表慢启动阶段的交互次数 
int ssthresh=3,cur=1,base=1;

void maintain_sb()
{
	while(ack_state[sendbase])
	{
		sendbase++;
	}
}

//握手发射器 
void _rdt_send(string flag)
{
	string s = "";
	package p(SourcePort, SourcePort, flag, packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

int ee=0;
//数据包发射器 
//根据id计算seqnum ,cur代表当前的packNum 
void rdt_send(string s, string packNum, bool end)
{
	if(s=="")ee++;
	string flag = match("");	
	 
//	cout<<"发送数据包，packNum: "<< to_dec(packNum)<<"\n"; 
	//最后一组要加标志位 
	if(end)flag[END]='1';

	package p(SourcePort, DesPort, flag, packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}
int dd;
void rdt_send(simple_packet p)
{
	if(p.data=="")dd++;
	rdt_send(p.data,p.packNum,p.end);
}

int reno_state;//0代表慢启动，1代表拥塞控制，2代表快速恢复 
int acknum;

//停等机制只保留快速重传部分 
void reno_FSM(int nxt_packnum)
{			
//	cout<<"nxt_packnum: "<<nxt_packnum<<"\n";
//	cout<<"acknum: "<<acknum<<"\n";
	
	if(acknum == nxt_packnum) //dup ACK
	{
		//引入快速重传:收到三个重复ack就会重传，不用等超时 
		if(dupACKcount==3&&!ack_state[nxt_packnum]){
				//cout<<"重传数据包："<<nxt_packnum<<"\n";
				rdt_send(unrecv[(nxt_packnum)%1001].pack);
		}	
	}
	else //new ack
	{
		acknum = nxt_packnum;
	}
}
void* recv_manager(void* args)
{			
	while(1)
	{		
		if(state!=1)continue;
		
		while(state!=1||file_que.empty());
		package p=file_que.front();
		file_que.pop();
		
		int nxt_packnum=stoi(to_dec(p.packNum));	

		if(check_lose(p))
		{
			int pid=stoi(to_dec(p.packNum));
			
			for(int i=sendbase;i<pid;++i)	
			{
				ack_state[i]=1;	
				valid[i]=0;				
			}	
								
			maintain_sb();			
			//维护状态机
			reno_FSM(nxt_packnum);		
		}
		else //差错重传
		{
			cout<<"发生差错！重传数据包："<<nxt_packnum<<"\n";
			rdt_send(sendData,p.packNum,p.flag[END]-'0');
		}
	}		
} 


//处理超时的线程 
//拥塞控制部分加入了相应状态转移函数 
void* timeout_handler(void* args)
{
	while(1)
	{
		Sleep(50);
		for(int i=sendbase;i<sendbase+win_size;++i)
		{			
			if(!valid[i]||ack_state[i])continue;
			
			int id=i;//%1001;
			int cur_pckn=unrecv[id].packNum;		
			clock_t cur_time=clock(); 
			
			//超时重传 				
			if((cur_time-unrecv[id].start)>100)
			{		
				cout<<"******************************************\n";			
				cout<<sendbase<<" "<<win_size<<"\n";
				cout<<"超时packnum: "<<cur_pckn<<"\n";
				cout<<"id: "<<id<<"\n";
				cout<<"重新发送数据包，packnum"<<cur_pckn<<"\n";
	
				unrecv[id].start=clock();	
				rdt_send(unrecv[id].pack);
				
				//超时状态转移	
//				reno_state=0;
//				ssthresh = win_size/2; 
//				win_size = 1; 
//				dupACKcount = 0;				
			}
		}	
	} 
	
}

node no;
simple_packet sp;

void send()
{	
	//先分组 
	groupNum = (sendData.size()+max_len-1)/max_len;
	vector<string> groupData;
	for (int i = 0; i < groupNum; ++i)
	{
		if (i < (groupNum - 1))groupData.push_back(sendData.substr(i * max_len, max_len));
		else groupData.push_back(sendData.substr(i * max_len));
	}
	
	cout<<"groupNum: "<<groupNum<<"\n";	
				
	//下面开始发送,根据sendbase和win_size确定可以发送的数据包的下标范围 
	int cur_packnum=sendbase,cnt=0,old_sendbase=sendbase;
	
	//窗口大小设置为 20 
	win_size=15;

	while(cur_packnum<old_sendbase+groupNum)
	{
		//可靠保证 
		if(win_size<=0)win_size=1;
		
		while(cur_packnum<(sendbase+win_size)&&cur_packnum<(old_sendbase+groupNum)) 
		{			
			//初始化一些相关参数 						
			sp.end=(cnt==(groupNum-1));	
			sp.data=groupData[cnt]; 
			sp.packNum=match(to_bin(to_string(cur_packnum)), 32);
			no.start=clock(); no.packNum= cur_packnum;	
			no.pack=sp;
			
			int id=cur_packnum;//%1001;
			
			//当前数据包设置成未收到模式 
			ack_state[id]=0;
			valid[id]=1;			
			unrecv[id]=no;
			//发送数据包		
			rdt_send(sp);
			//cout<<"cur_packnum: "<<cur_packnum<<"\n";			
			cnt++;cur_packnum++;
			
		}
	}
	cout<<win_size<<"\n";
	
	//防止超时重传部分没有处理完整个进城就退出 
	if(groupNum>10)Sleep(20000);
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
		Sleep(1000);
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
	Sleep(500);
	//第一次挥手(FIN=1，seq=x)            c.s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); 
	cout << "第一次挥手发送成功\n";
	
	
	//第二次挥手(ACK=1，ACKnum=x+1)       s.c
	while (file_que.empty());
	package p = file_que.front(); file_que.pop(); 
	//p.print();
	assert(p.flag[ACK] == '1');
	cout << "第二次挥手成功接收\n";

	//第三次挥手(FIN=1，seq=y)            s.c 
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	//p.print();	
	assert(p.flag[FIN] == '1');
	cout << "第三次挥手成功接收\n";
	
	Sleep(500);	
	//第四次挥手(ACK=1，ACKnum=y+1)       c.s
	flag[ACK] = '1'; _rdt_send(flag);
	cout << "第四次挥手发送成功\n";
	exit(0);
}


bool init()
{
	state = 0;//首先要设置当前状态为握手模式 
		 
	pic_set.insert("1.jpg");pic_set.insert("2.jpg");pic_set.insert("3.jpg");pic_set.insert("1.txt");
	pic_set.insert("small.png");pic_set.insert("mid.png");
	
	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		return 0;
	}
	sclient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	
	ssin.sin_family = AF_INET;
	ssin.sin_port = htons(stoi(DesPort));
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	len = sizeof(ssin);

	return 1;
}
int main(int argc, char* argv[])
{
	freopen("D:\\Desktop\\计网作业\\作业三\\NetworkHomework\\input.txt","r",stdin);
//	freopen("D:\\Desktop\\计网作业\\作业三\\NetworkHomework\\client.txt","w",stdout);
	cout<<"please input the source port: ";
	cin>>DesPort;
	
	Sleep(1000);

	if (!init())return 0;

	//接受信息新开一个线程
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	
	//处理信息的线程 
	pthread_t* thread2 = new pthread_t;
	pthread_create(thread2, NULL, recv_manager, NULL);
	
	//超时重传的线程 
	pthread_t* thread3 = new pthread_t;
	pthread_create(thread3, NULL, timeout_handler, NULL);
	 
	//三次握手 
	//connect();
	state=1;
	
	//每次发送信息都要走一遍状态机 
	send_manager();

	//四次挥手 
	//disconnect();
	cout<<"成功断开连接！\n"; 

	closesocket(sclient);
	WSACleanup();
	
	system("Pause");
	return 0;
}
