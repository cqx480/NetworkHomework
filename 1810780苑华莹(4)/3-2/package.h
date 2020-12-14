#include<bits/stdc++.h>
#include"bit_cal.h"
#include "common.h"
using namespace std;
const string s="8888";
const string d="8888";

struct package
{
	//������UDP����ʵ�ײ� 
	string srcPort, desPort;
	string len, check_sum;
	string data;

	//������TCP�������� 
	string flag;//16	
	string ackNum, seq;//32 32
	
	//���ݰ���id 
	string packNum;//32
	package(){}
	
	package(string fg, string ak, string se, string da)
	{
		srcPort = match(s);
		desPort = match(d);
		check_sum = match("");
		len = ceil(da.size() / 8.0);

		flag = fg;
		ackNum = match(ak,32);
		seq = match(se,32);
		packNum = match(se,32);
		data = da;
	}
	package(string s, string d, string fg, string ak, string se, string packN, string da)
	{
		srcPort = match(s);
		desPort = match(d);
		check_sum = match("");
		len = ceil(da.size() / 8.0);

		flag = fg;
		ackNum = match(ak,32);
		seq = match(se,32);
		
		packNum=match(packN,32);
		data = da;
	}
	bool isACK(int t)
	{
		return flag[ACK]=='1'&&flag[ACK_GROUP]=='0'+t;
	}
	void print()
	{
		cout<<"SrcPort: "<<srcPort<<"\n";
		cout<<"desPort: "<<desPort<<"\n";
		cout<<"len: "<<len<<"\n";
		cout<<"check_sum: "<<check_sum<<"\n";
		cout<<"data: "<<data<<"\n";
		cout<<"flag: "<<flag<<"\n";
		cout<<"ackNum: "<<ackNum<<"\n";
		cout<<"packNum: "<<packNum<<"\n";
		cout<<"seq: "<<seq<<"\n";
	}
	
	int ackgroup()
	{
		return flag[ACK_GROUP]-'0';
	}

};

//α�ײ� ,�ڳ�ʼ��α�ײ�֮ǰ��package�����Ѿ�������len 
struct fakeHead {
	string srcIP;
	string desIP;
	string zero, protocol, len;

	fakeHead()
	{
		srcIP = match("",32);
		desIP = match("",32);
		zero = match("",8); protocol = match("",8);
		len = match("");
	}
	//����192.22.22.1������ʽ��ip��ַ
	fakeHead(string s, string d)
	{
		srcIP = decode_ip(s);	//cout<<"srcIP: "<<srcIP<<endl;
		desIP = decode_ip(d);	//cout<<"desIP: "<<desIP<<endl;		
		zero = match("",8); protocol = match("",8);
		len = match("");
	}

	string decode_ip(string s)
	{
		string ans = "";
		int l = 0, r = 0;
		for (int i = 0; i < 4; ++i)
		{
			if (i < 3)r = s.find('.', l);
			else r = s.length();
			string tmp = s.substr(l, r - l);
			tmp = to_bin(tmp);
			ans += match(tmp,8);
			l = r + 1;
		}
		return match(ans,32);
	}
};


fakeHead f("127.0.0.1", "127.0.0.1");

//��������ȵ�16λstring 
void get_len(package& p)
{
	int len = ceil(p.data.length() / 8.0);
	string ans = to_string(len);
	ans = match(ans);
	p.len = ans;
}

//����У��� 
string get_twosum(string s1, string s2)
{
	string ans = "";
	assert(s1.length() == 16 && s2.length() == 16);

	int jin = 0;
	for (int i = 15; i >= 0; --i)
	{
		char cur = (jin + s1[i] - '0' + s2[i] - '0') % 2 + '0';
		ans += cur;
		jin = (jin + s1[i] - '0' + s2[i] - '0') / 2;
	}
	//��λҪ�ӻص�ĩλ 
	string tmp = "";
	if (jin) {
		for (int i = 15; i >= 0; --i)
		{
			char cur = (jin + ans[i] - '0') % 2 + '0';
			tmp += cur;
			jin = (jin + ans[i] - '0') / 2;
		}
		ans = tmp;
	}
	reverse(ans.begin(), ans.end());
	ans = match(ans);
	return ans;
}
//��������У��ͣ�0)�Լ����ȵ�package;




void get_sum(fakeHead f, package& p)
{
	string ans = ""; f.len = p.len;
	//α�ײ� 
	ans = get_twosum(f.srcIP.substr(0, 16), f.srcIP.substr(16));
	ans = get_twosum(ans, f.desIP.substr(0, 16));
	ans = get_twosum(ans, f.desIP.substr(16));
	ans = get_twosum(ans, f.zero + f.protocol);
	ans = get_twosum(ans, f.len);

	//tcp���� 
	ans = get_twosum(ans, p.srcPort);
	ans = get_twosum(ans, p.desPort);
	ans = get_twosum(ans, p.len);
	ans = get_twosum(ans, p.check_sum);

	ans = get_twosum(ans, p.flag);
	ans = get_twosum(ans, p.ackNum.substr(0, 16));
	ans = get_twosum(ans, p.ackNum.substr(16));
	ans = get_twosum(ans, p.seq.substr(0, 16));
	ans = get_twosum(ans, p.seq.substr(16));


	//���ݶ�Ϊ�˸�����㣬����ǿ��ת����int�ķ�ʽ 
	short int*data=(short int*)&p.data[0];
	unsigned short int cur_ans=0;
	for(int i=0;i<p.data.size()/2;++i)
	{
		cur_ans+=data[i];
		//while(cur_ans>=(1<<16))cur_ans>>=1;
	}
	
	string data_sum=match(to_string(cur_ans));
	
	ans = get_twosum(ans,data_sum);
	
	//���� 
	for (int i = 0; i < 16; ++i)ans[i] = '1' - ans[i] + '0';
	
	p.check_sum = ans;
}
//α�ײ�ֻ����������У��ͣ����������package 
string encode(package& p)
{
	string ans = "";
	ans += match(p.srcPort);
	ans += match(p.desPort);

	//���㳤�� 
	get_len(p);
	ans += match(p.len);

	//����У��� 
	get_sum(f, p);
	ans += match(p.check_sum);


	ans += p.flag;
	ans += p.ackNum;
	ans += p.seq;
	ans += p.packNum;
	ans += p.data;
	return ans;
}

void decode(string s, package& p)
{
	p.srcPort = s.substr(0, 16);
	p.desPort = s.substr(16, 16);
	p.len = s.substr(32, 16);
	p.check_sum = s.substr(48, 16);
	p.flag = s.substr(64, 16);
	p.ackNum = s.substr(80, 32);
	p.seq = s.substr(112, 32);
	p.packNum = s.substr(144, 32);
	p.data = s.substr(176);
}


bool check_lose(package p)
{
	//����ɵ�У��� 
	f.len=p.len;
	string olds = p.check_sum;
	p.check_sum = match("");

	get_sum(f, p);
	string news = p.check_sum;

	return news == olds;
}








