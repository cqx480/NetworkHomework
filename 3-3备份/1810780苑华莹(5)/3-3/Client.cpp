#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

#define MSS max_len

//数据部分最大长度
const int max_len = 14000; 

//sendbase最大值 
const int maxn=15000;
 
//整个数据包的最大长度 
const int N=15000;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state代表当前所处状态（0：握手阶段   1：数据发送阶段   2：挥手阶段） 
int state;

//seq是sendbase对应的序列号，ack是累计确认序列号 
string ack = match("", 32), seq = match("", 32),packNum = match("", 32);
int seqnum, acknum; 

//ack_state[i]代表packNum=1的数据包是否成功接收 
int ack_state[maxn];
int valid[maxn];


//待发送的字符串 
string sendData;

//输入流 
char recvData[N];

//源端口，目的端口 
string SourcePort="8888",DesPort;

//接收缓冲区 
queue<package> file_que;

//文件名称 
set<string>pic_set;


int groupNum;

void timeout_handler();

struct simple_packet{
	string data;
	string seq;
	string packNum;
	bool end;
	simple_packet(){}
	simple_packet(string d,string s,string p,bool e){
		data=d;
		seq=s;
		packNum=p;
		end=e;
	}
};

struct node{
	int packNum;
	clock_t start;
	simple_packet pack;
	node(){}
	node(int a,clock_t c,simple_packet p):packNum(a),start(c),pack(p){} 
	
	void print()
	{		
		cout<<"start: "<<start<<"\n";
		cout<<"pack.data: "<<pack.data<<"\n";
		cout<<"pack.seq: "<<pack.seq<<"\n";
		cout<<"pack.packNum: " <<pack.packNum<<"\n";
		cout<<"pack.end: "<<pack.end<<"\n";		 
	}
};

//当前窗口未确认的packnum和相关信息 
node unrecv[maxn];


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
//窗口大小的阈值设置为64 ,cur代表慢启动阶段的交互次数 
int ssthresh=64,cur=1,base=1;

void maintain_sb()
{
	while(ack_state[sendbase]){sendbase++;	}
}

//握手阶段seqnum每次加一 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;

	package p(SourcePort, SourcePort, flag, ack, seq,packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

//根据id计算seqnum ,cur代表当前的packNum 
void rdt_send(string s, string seq,string packNum, bool end)
{
	string flag = match("");	
	 
	//最后一组要加标志位 
	if(end)flag[END]='1';

	//seqnum += s.size() / 8;
	//maintain_as();

	package p(SourcePort, DesPort, flag, ack, seq, packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

void rdt_send(simple_packet p)
{
	rdt_send(p.data,p.seq,p.packNum,p.end);
}

int reno_state;//0代表慢启动，1代表拥塞控制，2代表快速恢复 

void reno_FSM(int nxt_packnum)
{
	//全局acknum 			
	//dup ACK
	if(acknum == nxt_packnum){
		if(reno_state==0||reno_state==1)
		{
			dupACKcount++;
			if(dupACKcount==3)
			{
				ssthresh= win_size/2;
				win_size = ssthresh + 3;
				reno_state=2;//状态机状态由 "慢启动"或"拥塞控制" 变 "快速恢复" 
			}
		}
		else if(reno_state==2)
		{
			win_size++;
		}
	}
	else //new ack
	{
		acknum = nxt_packnum;
		if(reno_state==0)
		{
			reno_state=1; //状态机状态由"慢启动"变"拥塞避免" 
			acknum = nxt_packnum;
			dupACKcount=0;
			if(win_size<ssthresh)win_size++;
		}
		else if(reno_state==1)
		{
			win_size = win_size + (MSS/win_size);//由于win_size本身就表示数据包的序号而不是字节的序号，因此不需要再乘以MSS 
			dupACKcount = 0 ;
		}
		else if(reno_state==2)
		{
			reno_state=1; //状态机状态由"慢启动"变"拥塞避免" 
			win_size = ssthresh;
			dupACKcount = 0 ;
		}
	}
}
void* recv_manager(void* args)
{			
	while(1)
	{		
		if(state!=1)continue;
		
		while(state!=1||file_que.empty());
		
		package p=file_que.front();file_que.pop();

		int cur_ack=stoi(to_dec(p.ackNum)),nxt_packnum=stoi(to_dec(p.seq));	

		if(check_lose(p))
		{
			int pid=stoi(to_dec(p.packNum));
			//cout<<"接收到消息 pid: "<<pid<<"\n";				
			ack_state[pid]=1;	
			valid[pid]=0;			
			maintain_sb();			
			//维护状态机
			reno_FSM(nxt_packnum);		
		}
		else //差错重传
		{
			cout<<"发生差错！重传数据包："<<cur_ack<<"\n";
			rdt_send(sendData,p.ackNum,p.packNum,p.flag[END]-'0');
		}
	}		
} 


//处理超时的线程 
//拥塞控制部分加入了相应状态转移函数 
void* timeout_handler(void* args)
{
	while(1)
	{
		Sleep(1000);
		for(int i=sendbase;i<=sendbase+win_size;++i)
		{	
			//没有这个package或者没有接受 
			//Sleep(100);
			if(!valid[i]||ack_state[i])continue;
			
			int cur_pckn=unrecv[i].packNum;		
			clock_t cur_time=clock(); 
			
			//超时重传 	
			
			if((cur_time-unrecv[i].start)>1000)
			{	
				cout<<"超时packnum: "<<unrecv[i].packNum<<"\n";
				cout<<"当前时间： "<<cur_time<<"\n";
				cout<<"pack start time: "<< unrecv[i].start<<"\n";
				unrecv[i].start=clock();		
				rdt_send(unrecv[i].pack);
				
				//超时状态转移	
				reno_state=0;
				ssthresh = win_size/2; 
				win_size = 1; 
				dupACKcount = 0;				
			}
		}	
	} 
	
}




 
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
	//窗口初始大小为 1 
	win_size=1;

	while(cur_packnum<old_sendbase+groupNum)
	{
		while(cur_packnum<(sendbase+win_size)&&cur_packnum<(old_sendbase+groupNum)) 
		{
			
			if(cur_packnum>1300)
			{
				cout<<"\n";
			}
			//初始化一些相关参数 		
			string curpackNum= match(to_bin(to_string(cur_packnum)), 32);			
			bool end=(cnt==(groupNum-1));								
			simple_packet sp(groupData[cnt],seq,curpackNum,end);
			node no(cur_packnum,clock(),sp);
			
			//当前数据包设置成未收到模式 
			ack_state[cur_packnum]=0;
			valid[cur_packnum]=1;			
			unrecv[cur_packnum]=no;
			
			//发送数据包		
			rdt_send(sp);
			cout<<"cur_packnum: "<<cur_packnum<<"\n";			
			cnt++;cur_packnum++;
			
		}
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
	//第一次挥手(FIN=1，seq=x)            c->s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); 
	cout << "第一次挥手发送成功\n";
	
	
	//第二次挥手(ACK=1，ACKnum=x+1)       s->c
	while (file_que.empty());
	package p = file_que.front(); file_que.pop(); 
	//p.print();
	assert(p.flag[ACK] == '1');
	cout << "第二次挥手成功接收\n";

	//第三次挥手(FIN=1，seq=y)            s->c 
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	//p.print();	
	assert(p.flag[FIN] == '1');
	cout << "第三次挥手成功接收\n";
	
	Sleep(500);	
	//第四次挥手(ACK=1，ACKnum=y+1)       c->s
	flag[ACK] = '1'; _rdt_send(flag);
	cout << "第四次挥手发送成功\n";
	exit(0);
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
	ssin.sin_port = htons(stoi(DesPort));
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	len = sizeof(ssin);

	return 1;
}
int main(int argc, char* argv[])
{
	//freopen("D:\\Desktop\\计网作业\\作业三\\NetworkHomework\\input.txt","r",stdin);
	cout<<"please input the source port: ";
	cin>>DesPort;
	
	Sleep(2000);

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
	connect();
	
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
