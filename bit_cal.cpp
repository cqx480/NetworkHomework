#include<bits/stdc++.h>
using namespace std;
	
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

