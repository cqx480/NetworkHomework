#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

//���ݲ�����󳤶�
const int max_len = (1e4)-1000; 

//sendbase���ֵ 
const int maxn=1e5+10;
 
//�������ݰ�����󳤶� 
const int N=1e4;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state������ǰ����״̬��0�����ֽ׶�   1�����ݷ��ͽ׶�   2�����ֽ׶Σ� 
int state;

//seq��sendbase��Ӧ�����кţ�ack���ۼ�ȷ�����к� 
string ack = match("", 32), seq = match("", 32),packNum = match("", 32);
int seqnum, acknum; 

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

//���ڼ�ʱ 
clock_t start[maxn];

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

//��ǰ����δȷ�ϵ�packnum�������Ϣ 
//map<int,node> unrecv;
node unrecv[maxn];


//�Ѿ��յ�������ack�����ֵ 
int sendbase;

//���ڴ�СΪ10 
const int win_size=10;


//ά��ȫ��ack��seq
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

			//�����ļ���
			//p.print();	
			file_que.push(p);
			
		}
	}
}

void maintain_sb()
{
	while(ack_state[sendbase])sendbase++;
}

//���ֽ׶�seqnumÿ�μ�һ 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;
	maintain_as();

	package p(SourcePort, SourcePort, flag, ack, seq,packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

//����id����seqnum ,cur������ǰ��packNum 
void rdt_send(string s, string seq,string packNum, bool end)
{
	string flag = match("");	
	 
	//���һ��Ҫ�ӱ�־λ 
	if(end)flag[END]='1';

	seqnum += s.size() / 8;
	maintain_as();

	package p(SourcePort, DesPort, flag, ack, seq, packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

void rdt_send(simple_packet p)
{
	rdt_send(p.data,p.seq,p.packNum,p.end);
}

void* recv_manager(void* args)
{			
	while(1)
	{		
		if(state!=1)continue;
		while(file_que.empty());
		package p=file_que.front();file_que.pop();
		
		int cur_ack=stoi(to_dec(p.ackNum)),cur_packnum=stoi(to_dec(p.packNum));	
		cout<<cur_packnum<<"\n";
		if(check_lose(p))
		{
			for(int i=0;i<5;++i){
				ack_state[cur_packnum+i]=1;			
				valid[cur_packnum+i]=0;
			}
			maintain_sb();
		}
		else //����ش�
		{
			rdt_send(sendData,p.ackNum,p.packNum,p.flag[END]-'0');
		}
	}		
} 


//������ʱ���߳� 
void* timeout_handler(void* args)
{
	while(1)
	{
		Sleep(500);
		for(int i=0;i<maxn;++i)
		{	
			//û�����package����û�н��� 
			Sleep(100);
			if(!valid[i]||ack_state[i])continue;
			
			int cur_pckn=unrecv[i].packNum;		
			clock_t cur_time=clock(); 
			
			//��ʱ�ش� 			
			if((cur_time-unrecv[i].start)>1000)
			{	
				cout<<"��ţ� "<<i<<"\n";
				cout<<"��ʱpacknum: "<<unrecv[i].packNum<<"\n";
				cout<<"��ǰʱ�䣺 "<<cur_time<<"\n";
				cout<<"pack start time: "<< unrecv[i].start<<"\n";
				unrecv[i].start=clock();		
				rdt_send(unrecv[i].pack);
			}
		}	
	} 
	
}


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
				
	//���濪ʼ����,����sendbase��win_sizeȷ�����Է��͵����ݰ����±귶Χ 
	int cur_packnum=sendbase,cnt=0;
	int old_sendbase=sendbase;
	while(cur_packnum<old_sendbase+groupNum)
	{
		while(cur_packnum<(sendbase+win_size)&&cur_packnum<(old_sendbase+groupNum)) 
		{
			int cur_seqnum = seqnum + cnt * max_len;
			
			string seq = match(to_bin(to_string(cur_seqnum)), 32);			
			string curpackNum= match(to_bin(to_string(cur_packnum)), 32);			
			bool end=(cnt==(groupNum-1));			
					
			simple_packet sp(groupData[cnt],seq,curpackNum,end);
			
			node no(cur_packnum,clock(),sp);
			ack_state[cur_packnum]=0;
			valid[cur_packnum]=1;
			
			unrecv[cur_packnum]=no;
					
			rdt_send(sp);
						
			cnt++;cur_packnum++;
		}
	}
	
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
		//���Ͽ����� 
		if (sendData == "q")
		{
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
	//��һ�λ���(FIN=1��seq=x)            c->s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); 
	cout << "��һ�λ��ַ��ͳɹ�\n";

	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	while (file_que.empty());
	package p = file_que.front(); file_que.pop(); 
	p.print();
	assert(p.flag[ACK] == '1');
	cout << "�ڶ��λ��ֳɹ�����\n";

	//�����λ���(FIN=1��seq=y)            s->c 
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	p.print();	
	assert(p.flag[FIN] == '1');
	cout << "�����λ��ֳɹ�����\n";

	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
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
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	len = sizeof(ssin);

	return 1;
}
int main(int argc, char* argv[])
{
	//freopen("D:\\Desktop\\������ҵ\\��ҵ��\\NetworkHomework\\input.txt","r",stdin);
	cout<<"please input the source port: ";
	cin>>DesPort;
	
	Sleep(500);

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
	disconnect();

	closesocket(sclient);
	WSACleanup();
	
	system("Pause");
	return 0;
}