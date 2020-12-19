#ifndef BIT_CAL.H
#define BIT_CAL.H

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
//对齐 
string match(string s,int Len=16)
{
	if(s.length() == Len)return s;
	if(s.length() > Len) return s.substr(0,Len);//取前Len位
	int len=s.length(),le=Len-len;
	string ans="";
	for(int i=0;i<le;++i)ans+="0";
	ans+=s;
	return ans;
} 
#endif
