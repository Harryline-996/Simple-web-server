//请注意！存放资源的文件夹为Webserver_root,含有txt,img,html三个子文件夹，需要手动将Webserver_root文件夹复制到D盘下


#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#pragma comment(lib,"ws2_32.lib")	//链接ws2_32.lib库
#include <winsock2.h>
#include <stdlib.h>
#include <conio.h>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <fstream>
using namespace std;

#define SERVER_PORT	6148 //侦听端口
#define LOGIN "3180106148"
#define PASS "6148"
#define BUFFERSIZE 8192
#define UP         1
#define DOWN       0

//定义客户端列表表项的格式
struct Client_info {
	int Cli_id;
	int Cli_state;	
	SOCKET Cli_socket;
	int Cli_port;
	string Cli_ip;
};

vector<Client_info>	Clientlist;	//定义全局变量Clientlist保存客户端列表
int Clientnum = 0;	//Clientnum保存客户端列表中的元素个数
int shouldquit = 0;
mutex Lock_for_Clientlist;	//定义一个互斥锁，限制各个线程对于Clientlist的访问
mutex Lock_for_shouldquit;	//定义一个互斥锁，限制各个线程对于shouldquit的访问

void sig_handler(int sig)
{
	if (sig == SIGINT)
	{
		{
			lock_guard<mutex> lk(Lock_for_shouldquit);
			shouldquit = 1;
		}

	}
}

//子线程执行的函数，用于与已经连接的客户端的后续交互
void Interact_with_Client(Client_info curclient) {
	char* request = new char[BUFFERSIZE];		//接收缓冲区
	char* respond = new char[BUFFERSIZE];		//发送缓冲区

	SOCKET now_socket = curclient.Cli_socket;
	int now_id = curclient.Cli_id;
	fstream file;			//文件标志符
	int enter;
	string url, root_path, method, version;    
	root_path = "D:/Webserver_root";		   //Webserver保存文件的根目录

	int ret = recv(now_socket, request, BUFFERSIZE, 0);
	while(1) {
		if (ret == SOCKET_ERROR)//接收失败
		{
			cout << "##### Error: recv()函数执行出错! #####" << endl;
			lock_guard<mutex> lk(Lock_for_Clientlist);
			Clientlist[now_id - 1].Cli_state = DOWN;
			closesocket(now_socket);//关闭套接字
			break;
		}
		else {
			string HTTP_request(request);
			stringstream header;
			header.str(HTTP_request);
			//查找CRLF（回车换行）
			if (HTTP_request.find("\r\n") != HTTP_request.npos) {		
				//查找是否是GET方法
				if (HTTP_request.find("GET") != HTTP_request.npos) {	
					//解析HTTP请求头
					header >> method >> url >> version;
					//分析url
					if (url == "/QUIT") {

						cout << "##### 客户端发送了退出信号！#####" << endl;
						{
							lock_guard<mutex> lk(Lock_for_Clientlist);
							Clientlist[now_id - 1].Cli_state = DOWN;
						}
						closesocket(now_socket);//关闭套接字
						{
							lock_guard<mutex> lk(Lock_for_shouldquit);
							shouldquit = 1;
						}
						break;
					}
					string file_path;
					file_path = root_path + url;
					cout << "##### "<< now_id << "号客户端发来GET请求！#####" << endl;
					cout << "##### FILEPATH:" << file_path << " #####" << endl;

					//打开文件
					file.open(file_path, std::ios::binary | std::ios::in);

					//如果请求的文件不存在
					if (!file)	
					{
						cout << "##### ERROR: No such file existed: " + file_path + "! #####" << endl;
						string error_info;	//存储响应消息
						error_info = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 84\r\n\r\n";
						error_info += "<html><body><h1>404 Not Found</h1><p>From server: URL is wrong.</p></body></html>\r\n";
						strcpy(respond, error_info.c_str());
						send(now_socket, respond, BUFFERSIZE, 0);
						closesocket(now_socket);
						break;
					}
					//请求的文件存在
					else {
						string response_header;		//保存HTML响应头
						response_header = "HTTP/1.1 200 OK\r\nContent-Type: ";

						long left, right;
						left = file.tellg();					//获取当前读指针位置保存到left
						file.seekg(0, ios::end);				//定位读指针位置为文件结尾
						right = file.tellg();					//获取当前读指针位置保存到right
						char* content = new char[right - left];
						file.seekg(0, ios::beg);				//重新定位读指针位置为文件开头
						file.read(content, right - left);		//读取文件内容
						file.close();							//关闭文件

						string type;
						if (url.find(".txt") != url.npos) {			//文本文件
							type = "text/plain;charset=utf-8\r\n";
							response_header += type;
						}
						else if (url.find(".html") != url.npos) {	//HTML文件
							type = "text/html;charset=utf-8\r\n";
							response_header += type;
						}
						else if (url.find(".jpg") != url.npos) {	//jpg图片文件
							type = "image/jpeg\r\n";
							response_header += type;
						}
						else {
							type = "none";
						}

						if (type != "none") {
							response_header += "Content-Length: ";
							ostringstream ss;
							ss << right - left;
							string length = ss.str();
							response_header += length;
							response_header += "\r\n\r\n";					//两个回车换行表示头部结束
							int size = response_header.length();
							memcpy(respond, response_header.c_str(), size);//头部信息
							memcpy(respond + size, content, right - left);//文件信息

							send(now_socket, respond, BUFFERSIZE, 0);
							closesocket(now_socket);
							break;
						}
					}
				}			

				//查找是否是POST方法
				if (HTTP_request.find("POST") != HTTP_request.npos) {
					string response_body;
					string response_head;
					header >> method >> url >> version;
					//输出提示信息
					cout << "##### " << now_id << "号客户端发来POST请求！#####" << endl;
					cout << "##### URL:" << url << " #####" << endl;

					//如果路径名不是dopost
					if (url != "/dopost") {
						cout << "##### ERROR: 路径名不是dopost！ #####" << endl;
						response_head = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 0\r\n\r\n";
						memcpy(respond, response_head.c_str(), 85);
						send(now_socket, respond, BUFFERSIZE, 0);
						closesocket(now_socket);
						break;
					}

					//如果路径名是dopost
					else {
						int cont_len = HTTP_request.find("Content-Length:");
						string tmp;
						header.clear();
						header.str(HTTP_request.substr(cont_len));	
						header >> tmp >> cont_len;					//抽取content-length

						int start = HTTP_request.find("\r\n\r\n");
						if (start != HTTP_request.npos) {
							string body;
							body = HTTP_request.substr(start + 4, cont_len);	//获取体部

							int pos1 = body.find("login=");
							int pos2 = body.find("&pass=");
							if (pos1 != body.npos && pos2 != body.npos && pos1 < pos2) {
								string login = body.substr(pos1 + 6, pos2 - pos1 - 6);
								string pswd = body.substr(pos2 + 6);

								if (login == LOGIN && pswd == PASS) {			//登录成功
									response_body = "<html><body>Login Success, Congratulations!</body></html>";
									response_head = "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 57\r\n\r\n";
								}
								else {											//登录失败
									response_body = "<html><body>Login Failed, Please check your account and password!</body></html>";
									response_head = "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 79\r\n\r\n";
								}

								response_head += response_body;
								int size = response_head.length();
								memcpy(respond, response_head.c_str(), size);
								send(now_socket, respond, BUFFERSIZE, 0);				//发送响应报文
								closesocket(now_socket);
								break;
							}
						}
					}

				}
			}
		}
		memset(request, 0, BUFFERSIZE);
		memset(respond, 0, BUFFERSIZE);
		ret = recv(now_socket, request, BUFFERSIZE, 0);

	}
	delete[] request;
	delete[] respond;
}


void main()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;
	int addrlen;
	SOCKET sListen, sServer; //侦听套接字，连接套接字
	struct sockaddr_in saServer, saClient;//地址信息
	Client_info tempinfo;

	//注册处理ctrl-c信号的函数
	signal(SIGINT, sig_handler);

	cout << "----------------------------------------------------" << endl;
	cout << "           欢迎使用我的轻量级Web服务器！" << endl;
	cout << "Director: LiXiang            Student ID: 3180106148" << endl;
	cout << "----------------------------------------------------" << endl;
	cout << endl;

	//WinSock初始化：
	wVersionRequested = MAKEWORD(2, 2);//希望使用的WinSock DLL的版本
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0)
	{
		cout << "##### WSAStartup()函数执行出错! #####" << endl;
		return;
	}
	//确认WinSock DLL支持版本2.2：
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		cout << "##### 当前选择的 Winsock 版本无效! #####" << endl;
		return;
	}

	//创建socket，使用TCP协议：
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		WSACleanup();
		cout << "##### socket()函数执行出错! #####" << endl;
		return;
	}

	//构建本地地址信息：
	saServer.sin_family = AF_INET;//地址家族
	saServer.sin_port = htons(SERVER_PORT);//注意转化为网络字节序
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//使用INADDR_ANY指示任意地址

	//绑定：
	ret = bind(sListen, (struct sockaddr*)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR)
	{
		cout << "##### bind()函数执行出错! 错误代码: " << WSAGetLastError() << " #####" << endl;
		closesocket(sListen);//关闭套接字
		WSACleanup();
		return;
	}

	//侦听连接请求：
	ret = listen(sListen, 5);
	if (ret == SOCKET_ERROR)
	{
		cout << "##### listen() failed! 错误代码: " << WSAGetLastError() << " #####" << endl;
		closesocket(sListen);//关闭套接字
		WSACleanup();
		return;
	}

	cout << "##### 服务端初始化成功，等待连接中... #####" << endl;
	addrlen = sizeof(saClient);

	//主线程循环调用accept()，阻塞等待接受客户端连接
	while (1) {
		sServer = accept(sListen, (struct sockaddr*)&saClient, &addrlen);
		//如果接收到的socket句柄无效，则继续循环
		if (sServer == INVALID_SOCKET)
			continue;
		//如果接收到有效的socket句柄，则在客户端列表中增加一个新客户端的项目，并记录下该客户端句柄和连接状态、端口，然后创建一个子线程后继续循环。
		else {
			//客户端列表中的元素个数加一
			Clientnum++;
			//记录下该客户端socket句柄、编号、ip地址、端口和连接状态，保存到tempinfo中
			tempinfo.Cli_socket = sServer;
			tempinfo.Cli_id = Clientnum;
			tempinfo.Cli_ip = inet_ntoa(saClient.sin_addr);
			tempinfo.Cli_port = ntohs(saClient.sin_port);
			tempinfo.Cli_state = UP;

			//将刚刚的记录加入Clientlist中
			Clientlist.push_back(tempinfo);

			//服务端输出相应的提示信息
			cout << endl;
			cout << "**********************************************************************" << endl;
			cout << "接受了" << tempinfo.Cli_id << "号客户端的连接请求！该客户端信息如下：" << endl;
			cout << "SOCKET：" << tempinfo.Cli_socket << endl;
			cout << "IP：" << tempinfo.Cli_ip << endl;
			cout << "PORT：" << tempinfo.Cli_port << endl;
			cout << "**********************************************************************\n" << endl;

			//创建一个子线程处理后续与该客户端的通信
			thread Childthread(Interact_with_Client, tempinfo);
			Childthread.detach();
		}

		{
			lock_guard<mutex> lk(Lock_for_shouldquit);
			if (shouldquit)
				break;
		}

	}

	shutdown(sListen, SD_BOTH);	//关闭socket的写端和读端
	closesocket(sListen);	//关闭套接字
	WSACleanup();			//终止Winsock 2 DLL 
	system("pause");
	return;

}





