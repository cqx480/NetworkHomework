#include <stdio.h>
#include <winsock2.h>
#include <pthread.h>

#include "common.h"
#include "package.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib") 

//���ݲ�����󳤶�
const int max_len = (1e4)-1000; 

//�������ݰ�����󳤶� 
const int N=1e4;

//socket 
SOCKET sclient;
sockaddr_in ssin;
int len;

//state����ǰ����״̬��0�����ֽ׶�   1�����ݷ��ͽ׶�   2�����ֽ׶Σ� 
int state;

//ȫ�����кź��ۼ�ȷ�����к� 
string ack = match("", 32), seq = match("", 32);
int seqnum, acknum; 

//�����͵��ַ��� 
string sendData;

//������ 
char recvData[N];

//Ŀ�Ķ˿� 
string SourcePort;

//���ջ����� 
queue<package> file_que;

//��ż���к� 
int t;

//�ļ����� 
set<string>pic_set;

//���ڼ�ʱ 
clock_t start,finish;

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
			file_que.push(p);
			memset(recvData,0,sizeof(recvData));
		}
	}
}
//���ֽ׶�seqnumÿ�μ�һ 
void _rdt_send(string flag)
{
	string s = "";

	seqnum += 1;
	maintain_as();

	package p(SourcePort, SourcePort, flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}
//t����ack_group��
void rdt_send(string s, int t,bool end)
{
	string flag = match("");
	flag[ACK] = '1'; 
	flag[ACK_GROUP] = '0' + t;
	
	//���һ��Ҫ�ӱ�־λ 
	if(end)flag[END]='1';

	seqnum += s.size() / 8;
	maintain_as();

	package p(SourcePort, SourcePort, flag, ack, seq, s);
	string sdata = encode(p);
	sendto(sclient, sdata.c_str(), sdata.size(), 0, (sockaddr*)&ssin, len);
}

void send()
{
	//���鷢��
	int groupNum = (sendData.size()+max_len-1)/max_len;
	string groupData;
	for (int i = 0; i < groupNum; ++i)
	{
		if (i < (groupNum - 1))groupData = sendData.substr(i * max_len, max_len);
		else groupData = sendData.substr(i * max_len);
		bool end=(i==groupNum-1);
		
		
		//���濪ʼ����ż���� 
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
//		cout<<"��"<<i+1<<"��ɹ�����\n"; 
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
	//��һ�λ���(FIN=1��seq=x)            c->s
	string flag = match(""); flag[FIN] = '1';
	_rdt_send(flag); flag[FIN] = '0';
	cout << "��һ�λ��ַ��ͳɹ�\n";

	//�ڶ��λ���(ACK=1��ACKnum=x+1)       s->c
	while (file_que.empty());
	cout << "qsize: " << file_que.size() << "\n";
	package p = file_que.front(); file_que.pop(); 
	assert(p.flag[ACK] == '1');
	cout << "�ڶ��λ��ֳɹ�����\n";

	//�����λ���(FIN=1��seq=y)            s->c 
	while (file_que.empty());
	cout << "qsize: " << file_que.size() << "\n";
	p = file_que.front(); file_que.pop();

	
	assert(p.flag[FIN] == '1');
	cout << "�����λ��ֳɹ�����\n";

	//���Ĵλ���(ACK=1��ACKnum=y+1)       c->s
	flag[ACK] = '1'; _rdt_send(flag);
	cout << "���Ĵλ��ַ��ͳɹ�\n";
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
	ssin.sin_port = htons(stoi(SourcePort));
	ssin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	len = sizeof(ssin);

	return 1;
}
int main(int argc, char* argv[])
{
	cout<<"please input the source port: ";
	cin>>SourcePort;
	
	//freopen("D:\\Desktop\\������ҵ\\��ҵ��\\NetworkHomework\\client_in.txt","w",stdout); 
	if (!init())return 0;

	//������Ϣ�¿�һ���߳�
	pthread_t* thread = new pthread_t;
	pthread_create(thread, NULL, receive, NULL);
	

	 
	//�������� 
	connect();
	
	//ÿ�η�����Ϣ��Ҫ��һ��״̬�� 
	send_manager();

	//�Ĵλ��� 
	disconnect();

	closesocket(sclient);
	WSACleanup();
	return 0;
}
