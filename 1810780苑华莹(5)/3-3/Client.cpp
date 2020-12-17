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

//���ݲ�����󳤶�
const int max_len = 12000; 

//sendbase���ֵ 
const int maxn=15000;
 
//�������ݰ�����󳤶� 
const int N=14000;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state����ǰ����״̬��0�����ֽ׶�   1�����ݷ��ͽ׶�   2�����ֽ׶Σ� 
int state;

string packNum = match("", 32);

//ack_state[i]����packNum=1�����ݰ��Ƿ�ɹ����� 
int ack_state[maxn];
int valid[maxn];


//�����͵��ַ��� 
string sendData;

//������ 
char recvData[N];

//Դ�˿ڣ�Ŀ�Ķ˿� 
string SourcePort="8888",DesPort;

//���ջ����� 
queue<package> file_que;

//�ļ����� 
set<string>pic_set;


int groupNum;

void timeout_handler();



//�Ѿ��յ�������ack�����ֵ 
int sendbase;

//��ʼ���ڴ�СΪ1
int win_size=1;

//�ظ�ack���� 
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

//����������ӵ�������㷨����ʼwin_size=1
//���ڴ�С����ֵ����Ϊ64 ,cur�����������׶εĽ������� 
int ssthresh=64,cur=1,base=1;

void maintain_sb()
{
	while(ack_state[sendbase])
	{
		sendbase++;
	}
}

//���ַ����� 
void _rdt_send(string flag)
{
	string s = "";
	package p(SourcePort, SourcePort, flag, packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

//���ݰ������� 
//����id����seqnum ,cur����ǰ��packNum 
void rdt_send(string s, string packNum, bool end)
{
	string flag = match("");	
	 
	cout<<"�������ݰ���packNum: "<< to_dec(packNum)<<"\n"; 
	//���һ��Ҫ�ӱ�־λ 
	if(end)flag[END]='1';

	package p(SourcePort, DesPort, flag, packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

void rdt_send(simple_packet p)
{
	rdt_send(p.data,p.packNum,p.end);
}

int reno_state;//0������������1����ӵ�����ƣ�2������ٻָ� 
int acknum;

void reno_FSM(int nxt_packnum)
{			
	
	if(acknum == nxt_packnum){ //dup ACK
		if(reno_state==0||reno_state==1)
		{
			dupACKcount++;
			if(dupACKcount==3)
			{
				ssthresh= win_size/2;
				win_size = ssthresh + 3;
				reno_state=2;//״̬��״̬�� "������"��"ӵ������" �� "���ٻָ�" 
			}
		}
		else if(reno_state==2)
		{
			if(win_size<5000)win_size++;
		}
	}
	else //new ack
	{
		acknum = nxt_packnum;
		if(reno_state==0)
		{
			reno_state=1; //״̬��״̬��"������"��"ӵ������" 
			acknum = nxt_packnum;
			dupACKcount=0;
			if(win_size<ssthresh)win_size++;
		}
		else if(reno_state==1)
		{
			if(win_size<5000)win_size++;//����win_size����ͱ�ʾ���ݰ�����Ŷ������ֽڵ���ţ���˲���Ҫ�ٳ���MSS 
			dupACKcount = 0 ;
		}
		else if(reno_state==2)
		{
			reno_state=1; //״̬��״̬��"������"��"ӵ������" 
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
		package p=file_que.front();
		file_que.pop();
		
		int nxt_packnum=stoi(to_dec(p.packNum));	

		if(check_lose(p))
		{
			int pid=stoi(to_dec(p.packNum));
//			cout<<"sendbase: "<<sendbase;
//			cout<<"   win_size: "<<win_size<<"\n"; 
//			cout<<"�������ݰ�: "<<pid<<"\n";
			for(int i=sendbase;i<=pid;++i)	
			{
				ack_state[i]=1;	
				valid[i]=0;				
			}						
			maintain_sb();			
			//ά��״̬��
			reno_FSM(nxt_packnum);		
		}
		else //����ش�
		{
			cout<<"��������ش����ݰ���"<<nxt_packnum<<"\n";
			rdt_send(sendData,p.packNum,p.flag[END]-'0');
		}
	}		
} 


//����ʱ���߳� 
//ӵ�����Ʋ��ּ�������Ӧ״̬ת�ƺ��� 
void* timeout_handler(void* args)
{
	while(1)
	{
		Sleep(1000);
		for(int i=sendbase;i<sendbase+win_size;++i)
		{			
			if(!valid[i]||ack_state[i])continue;
			
			int id=i%1001;
			int cur_pckn=unrecv[id].packNum;		
			clock_t cur_time=clock(); 
			
			//cout<<"aaaaaaaaaaaaaa unrecv["<<id<<"].packNum="<<cur_pckn<<"\n";
			
			//��ʱ�ش� 				
			if((cur_time-unrecv[id].start)>50)
			{		
//				cout<<"=======================\n";			
//				cout<<sendbase<<" "<<win_size<<"\n";
//				cout<<"��ʱpacknum: "<<cur_pckn<<"\n";
//				cout<<"id: "<<id<<"\n";
				unrecv[id].start=clock();		
				rdt_send(unrecv[id].pack);
				
				//��ʱ״̬ת��	
				reno_state=0;
				ssthresh = win_size/2; 
				win_size = 1; 
				dupACKcount = 0;				
			}
		}	
	} 
	
}

node no;
simple_packet sp;

void send()
{
	
	//�ȷ��� 
	groupNum = (sendData.size()+max_len-1)/max_len;
	vector<string> groupData;
	for (int i = 0; i < groupNum; ++i)
	{
		if (i < (groupNum - 1))groupData.push_back(sendData.substr(i * max_len, max_len));
		else groupData.push_back(sendData.substr(i * max_len));
	}
	
	cout<<"groupNum: "<<groupNum<<"\n";	
				
	//���濪ʼ����,����sendbase��win_sizeȷ�����Է��͵����ݰ����±귶Χ 
	int cur_packnum=sendbase,cnt=0,old_sendbase=sendbase;
	//���ڳ�ʼ��СΪ 1 
	win_size=1;

	while(cur_packnum<old_sendbase+groupNum)
	{
		while(cur_packnum<(sendbase+win_size)&&cur_packnum<(old_sendbase+groupNum)) 
		{			
			//��ʼ��һЩ��ز��� 		
				
			sp.end=(cnt==(groupNum-1));	sp.data=groupData[cnt]; 
			sp.packNum=match(to_bin(to_string(cur_packnum)), 32);
			no.start=clock(); no.packNum= cur_packnum;	
			no.pack=sp;
			
			int id=cur_packnum%1001;
			
			//��ǰ���ݰ����ó�δ�յ�ģʽ 
			ack_state[id]=0;
			valid[id]=1;			
			unrecv[id]=no;
			cout<<"cccccccccccccc unrecv["<<id<<"].packNum="<<cur_packnum<<"\n";
			//�������ݰ�		
			rdt_send(sp);
			//cout<<"cur_packnum: "<<cur_packnum<<"\n";			
			cnt++;cur_packnum++;
			
		}
	}
	cout<<win_size<<"\n";
} 

void get_pic()
{
	send();
	
	//������ļ���ַ 
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
		//���Ͽ����� 
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

//�������� 
void connect()
{
	//��һ������(SYN=1, seq=x)
	string flag = match(""); flag[SYN] = '1';
	_rdt_send(flag); flag[SYN] = '0';
	cout << "��һ�����ֳɹ�����\n";

	//�ڶ������� SYN=1, ACK=1, seq=y, ACKnum=x+1
	while (file_que.empty());
	package p = file_que.front(); file_que.pop();
	assert(p.flag[SYN] == '1' && p.flag[ACK] == '1');
	cout << "�ڶ������ֳɹ�����\n";

	//���������� ACK=1��ACKnum=y+1
	flag[ACK] = '1';
	_rdt_send(flag);
	cout << "���������ֳɹ�����\n";
	state = 1;
}

//�Ĵλ��� 
void disconnect()
{
	state = 2;	
	Sleep(500);
	//��һ�λ���(FIN=1��seq=x)            c.s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); 
	cout << "��һ�λ��ַ��ͳɹ�\n";
	
	
	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s.c
	while (file_que.empty());
	package p = file_que.front(); file_que.pop(); 
	//p.print();
	assert(p.flag[ACK] == '1');
	cout << "�ڶ��λ��ֳɹ�����\n";

	//�����λ���(FIN=1��seq=y)            s.c 
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	//p.print();	
	assert(p.flag[FIN] == '1');
	cout << "�����λ��ֳɹ�����\n";
	
	Sleep(500);	
	//���Ĵλ���(ACK=1��ACKnum=y+1)       c.s
	flag[ACK] = '1'; _rdt_send(flag);
	cout << "���Ĵλ��ַ��ͳɹ�\n";
	exit(0);
}


bool init()
{
	state = 0;//����Ҫ���õ�ǰ״̬Ϊ����ģʽ 
		 
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
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//inet_addr("10.134.146.124");
	len = sizeof(ssin);

	return 1;
}
int main(int argc, char* argv[])
{
	freopen("D:\\Desktop\\������ҵ\\��ҵ��\\NetworkHomework\\input.txt","r",stdin);
	cout<<"please input the source port: ";
	cin>>DesPort;
	
	Sleep(1000);

	if (!init())return 0;

	//������Ϣ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	
	//������Ϣ���߳� 
	pthread_t* thread2 = new pthread_t;
	pthread_create(thread2, NULL, recv_manager, NULL);
	
	//��ʱ�ش����߳� 
	pthread_t* thread3 = new pthread_t;
	pthread_create(thread3, NULL, timeout_handler, NULL);
	 
	//�������� 
	connect();
	
	//ÿ�η�����Ϣ��Ҫ��һ��״̬�� 
	send_manager();

	//�Ĵλ��� 
	//disconnect();
	cout<<"�ɹ��Ͽ����ӣ�\n"; 

	closesocket(sclient);
	WSACleanup();
	
	system("Pause");
	return 0;
}
