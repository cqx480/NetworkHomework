#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

#define MSS max_len

//���ݲ�����󳤶�
const int max_len = 14000; 

//sendbase���ֵ 
const int maxn=15000;
 
//�������ݰ�����󳤶� 
const int N=15000;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state����ǰ����״̬��0�����ֽ׶�   1�����ݷ��ͽ׶�   2�����ֽ׶Σ� 
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
node unrecv[maxn];


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
	while(ack_state[sendbase]){sendbase++;	}
}

//���ֽ׶�seqnumÿ�μ�һ 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;

	package p(SourcePort, SourcePort, flag, ack, seq,packNum, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

//����id����seqnum ,cur����ǰ��packNum 
void rdt_send(string s, string seq,string packNum, bool end)
{
	string flag = match("");	
	 
	//���һ��Ҫ�ӱ�־λ 
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

int reno_state;//0������������1����ӵ�����ƣ�2������ٻָ� 

void reno_FSM(int nxt_packnum)
{
	//ȫ��acknum 			
	//dup ACK
	if(acknum == nxt_packnum){
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
			win_size++;
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
			win_size = win_size + (MSS/win_size);//����win_size����ͱ�ʾ���ݰ�����Ŷ������ֽڵ���ţ���˲���Ҫ�ٳ���MSS 
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
		
		package p=file_que.front();file_que.pop();

		int cur_ack=stoi(to_dec(p.ackNum)),nxt_packnum=stoi(to_dec(p.seq));	

		if(check_lose(p))
		{
			int pid=stoi(to_dec(p.packNum));
			//cout<<"���յ���Ϣ pid: "<<pid<<"\n";				
			ack_state[pid]=1;	
			valid[pid]=0;			
			maintain_sb();			
			//ά��״̬��
			reno_FSM(nxt_packnum);		
		}
		else //����ش�
		{
			cout<<"��������ش����ݰ���"<<cur_ack<<"\n";
			rdt_send(sendData,p.ackNum,p.packNum,p.flag[END]-'0');
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
		for(int i=sendbase;i<=sendbase+win_size;++i)
		{	
			//û�����package����û�н��� 
			//Sleep(100);
			if(!valid[i]||ack_state[i])continue;
			
			int cur_pckn=unrecv[i].packNum;		
			clock_t cur_time=clock(); 
			
			//��ʱ�ش� 	
			
			if((cur_time-unrecv[i].start)>1000)
			{	
				cout<<"��ʱpacknum: "<<unrecv[i].packNum<<"\n";
				cout<<"��ǰʱ�䣺 "<<cur_time<<"\n";
				cout<<"pack start time: "<< unrecv[i].start<<"\n";
				unrecv[i].start=clock();		
				rdt_send(unrecv[i].pack);
				
				//��ʱ״̬ת��	
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
			
			if(cur_packnum>1300)
			{
				cout<<"\n";
			}
			//��ʼ��һЩ��ز��� 		
			string curpackNum= match(to_bin(to_string(cur_packnum)), 32);			
			bool end=(cnt==(groupNum-1));								
			simple_packet sp(groupData[cnt],seq,curpackNum,end);
			node no(cur_packnum,clock(),sp);
			
			//��ǰ���ݰ����ó�δ�յ�ģʽ 
			ack_state[cur_packnum]=0;
			valid[cur_packnum]=1;			
			unrecv[cur_packnum]=no;
			
			//�������ݰ�		
			rdt_send(sp);
			cout<<"cur_packnum: "<<cur_packnum<<"\n";			
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
	//��һ�λ���(FIN=1��seq=x)            c->s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); 
	cout << "��һ�λ��ַ��ͳɹ�\n";
	
	
	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	while (file_que.empty());
	package p = file_que.front(); file_que.pop(); 
	//p.print();
	assert(p.flag[ACK] == '1');
	cout << "�ڶ��λ��ֳɹ�����\n";

	//�����λ���(FIN=1��seq=y)            s->c 
	while (file_que.empty());
	p = file_que.front(); file_que.pop();
	//p.print();	
	assert(p.flag[FIN] == '1');
	cout << "�����λ��ֳɹ�����\n";
	
	Sleep(500);	
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
	
	Sleep(2000);

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
