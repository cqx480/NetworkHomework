#include <bits/stdc++.h>
using namespace std;
string fun(string a,int m)
{
    int len=a.size(),i, n=0,j=0;
    int num[1024];
    for(i=0;i<len;i++)
    {
        if(a[i]>>7&1)//Ӣ���ַ�ascii�붼С��128,�����ַ�����������ascii128���ֽ����  A1B0��ʾ"��"
        {
            num[n++]=i+2;//num����Ԫ�ر����ȡ�ַ����ĳ���,�������ַ�ת����һ���ַ���
            i++;
        }
        else
        {
            num[n++]=i+1;
        }
    }
    for(i=0;i<num[m-1];i++)
    {
        a[j++]=a[i];
    }
    return a;
}
int main()
{
	string s="bfhcdn�Ͳ��ֻƵ��ھ�fd";
	s=fun(s,8);
	cout<<s; 
}
