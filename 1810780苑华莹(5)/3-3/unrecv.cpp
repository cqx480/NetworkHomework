#include<bits/stdc++.h>
#include"package.h"
using namespace std;


struct simple_packet{
	string data;
	string packNum;
	bool end;
	simple_packet(){}
	simple_packet(string d,string p,bool e){
		data=d;
		packNum=p;
		end=e;
	}
};

struct node{
	int packNum;
	clock_t start;
	simple_packet pack;
	node(){}
	node(int a,clock_t c,simple_packet p):packNum(a),start(c),pack(p){} 
	
	void print()
	{		
		cout<<"start: "<<start<<"\n";
		cout<<"pack.data: "<<pack.data<<"\n";
		cout<<"pack.packNum: " <<pack.packNum<<"\n";
		cout<<"pack.end: "<<pack.end<<"\n";		 
	}
};
const int M=1e5+10;

//当前窗口未确认的packnum和相关信息 
node unrecv[M];


//接收缓冲区 
queue<package> file_que;
