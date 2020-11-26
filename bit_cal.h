#include<bits/stdc++.h>
using namespace std;

//将十进制转化为二进制
string to_bin(string num)
{
	int n=stoi(num);
	string ans="";
	while(n)
	{
		char c=n%2+'0';
		ans+=c;
		n/=2;
	}
	reverse(ans.begin(),ans.end());
	return ans;
}
//二进制转成十进制  00010->2
string to_dec(string num)
{
	int n=0,base=1;
	for(int i=num.size()-1;i>=0;--i)
	{
		n+=(num[i]-'0')*base;
		base*=2;
	} 
	string ans=to_string(n);
	return ans;
}	
//八位对齐 
string get_8(string s)
{
	if(s.length() == 8)return s;
	if(s.length() > 8) return s.substr(0,8);//取前八位
	int len=s.length(),le=8-len;
	string ans="";
	for(int i=0;i<le;++i)ans+="0";
	ans+=s;
	return ans;
} 

string get_16(string s)
{
	if(s.length() == 16)return s;
	if(s.length() > 16) return s.substr(0,16);//取前八位
	int len=s.length(),le=16-len;
	string ans="";
	for(int i=0;i<le;++i)ans+="0";
	ans+=s;
	return ans;
} 
string get_32(string s)
{
	if(s.length() == 32)return s;
	if(s.length() > 32) return s.substr(0,32);//取前八位
	int len=s.length(),le=32-len;
	string ans="";
	for(int i=0;i<le;++i)ans+="0";
	ans+=s;
	return ans;
} 

