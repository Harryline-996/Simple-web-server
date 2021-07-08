//��ע�⣡�����Դ���ļ���ΪWebserver_root,����txt,img,html�������ļ��У���Ҫ�ֶ���Webserver_root�ļ��и��Ƶ�D����


#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#pragma comment(lib,"ws2_32.lib")	//����ws2_32.lib��
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

#define SERVER_PORT	6148 //�����˿�
#define LOGIN "3180106148"
#define PASS "6148"
#define BUFFERSIZE 8192
#define UP         1
#define DOWN       0

//����ͻ����б����ĸ�ʽ
struct Client_info {
	int Cli_id;
	int Cli_state;	
	SOCKET Cli_socket;
	int Cli_port;
	string Cli_ip;
};

vector<Client_info>	Clientlist;	//����ȫ�ֱ���Clientlist����ͻ����б�
int Clientnum = 0;	//Clientnum����ͻ����б��е�Ԫ�ظ���
int shouldquit = 0;
mutex Lock_for_Clientlist;	//����һ�������������Ƹ����̶߳���Clientlist�ķ���
mutex Lock_for_shouldquit;	//����һ�������������Ƹ����̶߳���shouldquit�ķ���

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

//���߳�ִ�еĺ������������Ѿ����ӵĿͻ��˵ĺ�������
void Interact_with_Client(Client_info curclient) {
	char* request = new char[BUFFERSIZE];		//���ջ�����
	char* respond = new char[BUFFERSIZE];		//���ͻ�����

	SOCKET now_socket = curclient.Cli_socket;
	int now_id = curclient.Cli_id;
	fstream file;			//�ļ���־��
	int enter;
	string url, root_path, method, version;    
	root_path = "D:/Webserver_root";		   //Webserver�����ļ��ĸ�Ŀ¼

	int ret = recv(now_socket, request, BUFFERSIZE, 0);
	while(1) {
		if (ret == SOCKET_ERROR)//����ʧ��
		{
			cout << "##### Error: recv()����ִ�г���! #####" << endl;
			lock_guard<mutex> lk(Lock_for_Clientlist);
			Clientlist[now_id - 1].Cli_state = DOWN;
			closesocket(now_socket);//�ر��׽���
			break;
		}
		else {
			string HTTP_request(request);
			stringstream header;
			header.str(HTTP_request);
			//����CRLF���س����У�
			if (HTTP_request.find("\r\n") != HTTP_request.npos) {		
				//�����Ƿ���GET����
				if (HTTP_request.find("GET") != HTTP_request.npos) {	
					//����HTTP����ͷ
					header >> method >> url >> version;
					//����url
					if (url == "/QUIT") {

						cout << "##### �ͻ��˷������˳��źţ�#####" << endl;
						{
							lock_guard<mutex> lk(Lock_for_Clientlist);
							Clientlist[now_id - 1].Cli_state = DOWN;
						}
						closesocket(now_socket);//�ر��׽���
						{
							lock_guard<mutex> lk(Lock_for_shouldquit);
							shouldquit = 1;
						}
						break;
					}
					string file_path;
					file_path = root_path + url;
					cout << "##### "<< now_id << "�ſͻ��˷���GET����#####" << endl;
					cout << "##### FILEPATH:" << file_path << " #####" << endl;

					//���ļ�
					file.open(file_path, std::ios::binary | std::ios::in);

					//���������ļ�������
					if (!file)	
					{
						cout << "##### ERROR: No such file existed: " + file_path + "! #####" << endl;
						string error_info;	//�洢��Ӧ��Ϣ
						error_info = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 84\r\n\r\n";
						error_info += "<html><body><h1>404 Not Found</h1><p>From server: URL is wrong.</p></body></html>\r\n";
						strcpy(respond, error_info.c_str());
						send(now_socket, respond, BUFFERSIZE, 0);
						closesocket(now_socket);
						break;
					}
					//������ļ�����
					else {
						string response_header;		//����HTML��Ӧͷ
						response_header = "HTTP/1.1 200 OK\r\nContent-Type: ";

						long left, right;
						left = file.tellg();					//��ȡ��ǰ��ָ��λ�ñ��浽left
						file.seekg(0, ios::end);				//��λ��ָ��λ��Ϊ�ļ���β
						right = file.tellg();					//��ȡ��ǰ��ָ��λ�ñ��浽right
						char* content = new char[right - left];
						file.seekg(0, ios::beg);				//���¶�λ��ָ��λ��Ϊ�ļ���ͷ
						file.read(content, right - left);		//��ȡ�ļ�����
						file.close();							//�ر��ļ�

						string type;
						if (url.find(".txt") != url.npos) {			//�ı��ļ�
							type = "text/plain;charset=utf-8\r\n";
							response_header += type;
						}
						else if (url.find(".html") != url.npos) {	//HTML�ļ�
							type = "text/html;charset=utf-8\r\n";
							response_header += type;
						}
						else if (url.find(".jpg") != url.npos) {	//jpgͼƬ�ļ�
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
							response_header += "\r\n\r\n";					//�����س����б�ʾͷ������
							int size = response_header.length();
							memcpy(respond, response_header.c_str(), size);//ͷ����Ϣ
							memcpy(respond + size, content, right - left);//�ļ���Ϣ

							send(now_socket, respond, BUFFERSIZE, 0);
							closesocket(now_socket);
							break;
						}
					}
				}			

				//�����Ƿ���POST����
				if (HTTP_request.find("POST") != HTTP_request.npos) {
					string response_body;
					string response_head;
					header >> method >> url >> version;
					//�����ʾ��Ϣ
					cout << "##### " << now_id << "�ſͻ��˷���POST����#####" << endl;
					cout << "##### URL:" << url << " #####" << endl;

					//���·��������dopost
					if (url != "/dopost") {
						cout << "##### ERROR: ·��������dopost�� #####" << endl;
						response_head = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 0\r\n\r\n";
						memcpy(respond, response_head.c_str(), 85);
						send(now_socket, respond, BUFFERSIZE, 0);
						closesocket(now_socket);
						break;
					}

					//���·������dopost
					else {
						int cont_len = HTTP_request.find("Content-Length:");
						string tmp;
						header.clear();
						header.str(HTTP_request.substr(cont_len));	
						header >> tmp >> cont_len;					//��ȡcontent-length

						int start = HTTP_request.find("\r\n\r\n");
						if (start != HTTP_request.npos) {
							string body;
							body = HTTP_request.substr(start + 4, cont_len);	//��ȡ�岿

							int pos1 = body.find("login=");
							int pos2 = body.find("&pass=");
							if (pos1 != body.npos && pos2 != body.npos && pos1 < pos2) {
								string login = body.substr(pos1 + 6, pos2 - pos1 - 6);
								string pswd = body.substr(pos2 + 6);

								if (login == LOGIN && pswd == PASS) {			//��¼�ɹ�
									response_body = "<html><body>Login Success, Congratulations!</body></html>";
									response_head = "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 57\r\n\r\n";
								}
								else {											//��¼ʧ��
									response_body = "<html><body>Login Failed, Please check your account and password!</body></html>";
									response_head = "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\nContent-Length: 79\r\n\r\n";
								}

								response_head += response_body;
								int size = response_head.length();
								memcpy(respond, response_head.c_str(), size);
								send(now_socket, respond, BUFFERSIZE, 0);				//������Ӧ����
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
	SOCKET sListen, sServer; //�����׽��֣������׽���
	struct sockaddr_in saServer, saClient;//��ַ��Ϣ
	Client_info tempinfo;

	//ע�ᴦ��ctrl-c�źŵĺ���
	signal(SIGINT, sig_handler);

	cout << "----------------------------------------------------" << endl;
	cout << "           ��ӭʹ���ҵ�������Web��������" << endl;
	cout << "Director: LiXiang            Student ID: 3180106148" << endl;
	cout << "----------------------------------------------------" << endl;
	cout << endl;

	//WinSock��ʼ����
	wVersionRequested = MAKEWORD(2, 2);//ϣ��ʹ�õ�WinSock DLL�İ汾
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0)
	{
		cout << "##### WSAStartup()����ִ�г���! #####" << endl;
		return;
	}
	//ȷ��WinSock DLL֧�ְ汾2.2��
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		cout << "##### ��ǰѡ��� Winsock �汾��Ч! #####" << endl;
		return;
	}

	//����socket��ʹ��TCPЭ�飺
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		WSACleanup();
		cout << "##### socket()����ִ�г���! #####" << endl;
		return;
	}

	//�������ص�ַ��Ϣ��
	saServer.sin_family = AF_INET;//��ַ����
	saServer.sin_port = htons(SERVER_PORT);//ע��ת��Ϊ�����ֽ���
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//ʹ��INADDR_ANYָʾ�����ַ

	//�󶨣�
	ret = bind(sListen, (struct sockaddr*)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR)
	{
		cout << "##### bind()����ִ�г���! �������: " << WSAGetLastError() << " #####" << endl;
		closesocket(sListen);//�ر��׽���
		WSACleanup();
		return;
	}

	//������������
	ret = listen(sListen, 5);
	if (ret == SOCKET_ERROR)
	{
		cout << "##### listen() failed! �������: " << WSAGetLastError() << " #####" << endl;
		closesocket(sListen);//�ر��׽���
		WSACleanup();
		return;
	}

	cout << "##### ����˳�ʼ���ɹ����ȴ�������... #####" << endl;
	addrlen = sizeof(saClient);

	//���߳�ѭ������accept()�������ȴ����ܿͻ�������
	while (1) {
		sServer = accept(sListen, (struct sockaddr*)&saClient, &addrlen);
		//������յ���socket�����Ч�������ѭ��
		if (sServer == INVALID_SOCKET)
			continue;
		//������յ���Ч��socket��������ڿͻ����б�������һ���¿ͻ��˵���Ŀ������¼�¸ÿͻ��˾��������״̬���˿ڣ�Ȼ�󴴽�һ�����̺߳����ѭ����
		else {
			//�ͻ����б��е�Ԫ�ظ�����һ
			Clientnum++;
			//��¼�¸ÿͻ���socket�������š�ip��ַ���˿ں�����״̬�����浽tempinfo��
			tempinfo.Cli_socket = sServer;
			tempinfo.Cli_id = Clientnum;
			tempinfo.Cli_ip = inet_ntoa(saClient.sin_addr);
			tempinfo.Cli_port = ntohs(saClient.sin_port);
			tempinfo.Cli_state = UP;

			//���ոյļ�¼����Clientlist��
			Clientlist.push_back(tempinfo);

			//����������Ӧ����ʾ��Ϣ
			cout << endl;
			cout << "**********************************************************************" << endl;
			cout << "������" << tempinfo.Cli_id << "�ſͻ��˵��������󣡸ÿͻ�����Ϣ���£�" << endl;
			cout << "SOCKET��" << tempinfo.Cli_socket << endl;
			cout << "IP��" << tempinfo.Cli_ip << endl;
			cout << "PORT��" << tempinfo.Cli_port << endl;
			cout << "**********************************************************************\n" << endl;

			//����һ�����̴߳��������ÿͻ��˵�ͨ��
			thread Childthread(Interact_with_Client, tempinfo);
			Childthread.detach();
		}

		{
			lock_guard<mutex> lk(Lock_for_shouldquit);
			if (shouldquit)
				break;
		}

	}

	shutdown(sListen, SD_BOTH);	//�ر�socket��д�˺Ͷ���
	closesocket(sListen);	//�ر��׽���
	WSACleanup();			//��ֹWinsock 2 DLL 
	system("pause");
	return;

}





