#include<bits/stdc++.h>
#include"bit_cal.h"
#include "common.h"
using namespace std;

struct package
{
	//首先是UDP的真实首部 
	string srcPort,desPort;
	string len,check_sum;
	string data;
		
	//下面是TCP新增内容 
	string flag;//16	
	string ackNum,seq;//32 32
	package()
	{
		srcPort=desPort=check_sum=len=get_16("");
		flag=get_16(""); 
		ackNum=seq=get_32("");
	}
	
	package(string s,string d,string fg,string ak,string se,string da)
	{
		srcPort=get_16(to_bin(s));
		desPort=get_16(to_bin(d));
		check_sum=get_16("");
		len=ceil(da.size()/8.0);
		
		flag=fg;
		ackNum=get_32(to_bin(ak));
		seq=get_32(to_bin(se));
		
		data=da;
	}
};

//伪首部 ,在初始化伪首部之前，package必须已经计算完len 
struct fakeHead{
	string srcIP;
	string desIP;
	string zero,protocol,len;
	
	fakeHead() 
	{
		srcIP=get_32("");		
		desIP=get_32("");		
		zero=get_8("");protocol=get_8("");
		len=get_16("");
	}	
	//传入192.22.22.1这样格式的ip地址
	fakeHead(string s,string d) 
	{
		srcIP=decode_ip(s);	//cout<<"srcIP: "<<srcIP<<endl;
		desIP=decode_ip(d);	//cout<<"desIP: "<<desIP<<endl;		
		zero=get_8("");protocol=get_8("");
		len=get_16("");
	}	
		
	string decode_ip(string s)
	{
		string ans="";
		int l=0,r=0;
		for(int i=0;i<4;++i)
		{
			if(i<3)r=s.find('.',l);
			else r=s.length();
			string tmp=s.substr(l,r-l);
			tmp=to_bin(tmp); 
			ans+=get_8(tmp);
			l=r+1;
		}
		return ans;
	}
};



//计算代表长度的16位string 
void get_len(package& p)
{
	int len=ceil(p.data.length()/8.0);
	string ans=to_string(len);
	ans=get_16(ans);
	p.len=ans; 
}

//计算校验和 
string get_twosum(string s1,string s2)
{
	string ans="";
	assert(s1.length()==16&&s2.length()==16);
	
	int jin=0;
	for(int i=15;i>=0;--i)
	{
		char cur=(jin+s1[i]-'0'+s2[i]-'0')%2+'0';
		ans+=cur; 
		jin=(jin+s1[i]-'0'+s2[i]-'0')/2;
	}
	//进位要加回到末位 
	string tmp="";
	if(jin){		
		for(int i=15;i>=0;--i)
		{
			char cur=(jin+ans[i]-'0')%2+'0';
			tmp+=cur; 
			jin=(jin+ans[i]-'0')/2;
		}
		ans=tmp;
	} 
	reverse(ans.begin(),ans.end());
	ans=get_16(ans);
	//求反码 
	for(int i=0;i<16;++i)ans[i]='1'-ans[i]+'0'; 	
	return ans;
}
//传入计算好校验和（0)以及长度的package;
void get_sum(fakeHead f,package& p)
{
	string ans="";f.len=p.len; 
	//伪首部 
	ans=get_twosum(f.srcIP.substr(0,16),f.srcIP.substr(16));
	ans=get_twosum(ans,f.desIP.substr(0,16));
	ans=get_twosum(ans,f.desIP.substr(16));	
	ans=get_twosum(ans,f.zero+f.protocol);
	ans=get_twosum(ans,f.len);
	
	//tcp报文 
	ans=get_twosum(ans,p.srcPort);
	ans=get_twosum(ans,p.desPort);
	ans=get_twosum(ans,p.len);
	ans=get_twosum(ans,p.check_sum);
	
	ans=get_twosum(ans,p.flag); 
	ans=get_twosum(ans,p.ackNum.substr(0,16)); 
	ans=get_twosum(ans,p.ackNum.substr(16)); 
	ans=get_twosum(ans,p.seq.substr(0,16)); 
	ans=get_twosum(ans,p.seq.substr(16)); 
	
	//数据段: 每个字符对应的ASCII码转换成int再换成二进制进行计算
	for(int i=0;i<p.data.size();++i)
	{
		int dec_num=p.data[i];
		ans=get_twosum(ans,get_16(to_string(dec_num)));
	}
	p.check_sum=ans; 
} 
//伪首部只是用来计算校验和，并不打包进package 
string encode(fakeHead f,package& p)
{
	string ans="";
	ans+=get_16(p.srcPort);
	ans+=get_16(p.desPort);
	
	//计算长度 
	get_len(p);
	ans+=get_16(p.len);
	
	//计算校验和 
	get_sum(f,p);
	ans+=get_16(p.check_sum);
	
	
	ans+=p.flag;
	ans+=p.ackNum;
	ans+=p.seq;
	ans+=p.data;
	return ans;
}

void decode(string s,package& p)
{
	p.srcPort=s.substr(0,16);
	p.desPort=s.substr(16,16);
	p.len=s.substr(32,16);
	p.check_sum=s.substr(48,16);
	p.flag=s.substr(64,16);
	p.ackNum=s.substr(80,32);
	p.seq=s.substr(112,32);
	p.data=s.substr(144);	
}

//通过校验和判断是否有数据丢失 
bool check_lose(fakeHead f,package p)
{	
	//保存旧的校验和 
	string olds=p.check_sum;
	p.check_sum=get_16("");

	get_sum(f,p);
	string news=p.check_sum;
	
	return news==olds;
}








