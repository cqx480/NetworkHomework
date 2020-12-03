#include <bits/stdc++.h>
using namespace std;
string fun(string a,int m)
{
    int len=a.size(),i, n=0,j=0;
    int num[1024];
    for(i=0;i<len;i++)
    {
        if(a[i]>>7&1)//英文字符ascii码都小于128,中文字符由两个大于ascii128的字节组成  A1B0表示"啊"
        {
            num[n++]=i+2;//num数组元素保存截取字符串的长度,将中文字符转化成一个字符看
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
	string s="bfhcdn和部分黄帝内经fd";
	s=fun(s,8);
	cout<<s; 
}
