#include <bits/stdc++.h>
using namespace std;
string _substr(string s,int m)
{
    int cnt=0;
    for(int i=0;i<s.size();++i)
    {
    	if(cnt>=2*m)break;
    	if(s[i]>>7==1)cnt++;
    	else cnt+=2;
	}
	return s.substr(0,cnt);
}
int main()
{	
	string s="bf和部junhghny就那天刚好";
	cout<<s.size()<<endl;
	s=_substr(s,3);
	cout<<s; 
}
