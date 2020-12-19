#ifndef BIT_CAL.H
#define BIT_CAL.H

#include<bits/stdc++.h>
using namespace std;

//��ʮ����ת��Ϊ������
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
//������ת��ʮ����  00010->2
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
//���� 
string match(string s,int Len=16)
{
	if(s.length() == Len)return s;
	if(s.length() > Len) return s.substr(0,Len);//ȡǰLenλ
	int len=s.length(),le=Len-len;
	string ans="";
	for(int i=0;i<le;++i)ans+="0";
	ans+=s;
	return ans;
} 
#endif
