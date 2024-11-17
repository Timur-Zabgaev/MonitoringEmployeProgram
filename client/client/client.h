// client.h : включаемый файл для стандартных системных включаемых файлов
// или включаемые файлы для конкретного проекта.

#pragma once

#include <iostream>
#ifndef UNICODE
#define UNICODE
#endif
#pragma comment(lib, "netapi32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <netioapi.h>
#include <vector>
#include <windows.h>
#include <thread>
#include <iomanip>
#include <lm.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <bitset>
#include <mutex>

enum class  OPERATION_RESULT {
	SUCCESS = 0,
	FAILURE = -1,
};

class ClientHandler {
public:
	ClientHandler(const std::string& ip, uint16_t portSend, uint16_t portListen);
	OPERATION_RESULT clientConnect();
	OPERATION_RESULT sendMessage(const std::string& jsonData, const std::string& handle);
	std::string GetActivity();
	std::string GetIP();
	std::string GetClientMachine();
	std::string GetClientUserName();
	std::string GetDomainName();
	~ClientHandler();
	void sendRequests();
	void listenForRequests();
private:
	std::vector <BYTE> captureScreenToMemory();
	void MonitorActivity();
	void SetClientINFO();
	SOCKET sockToSend;
	SOCKET sockToListen;
	sockaddr_in serverAddrSend;
	sockaddr_in serverAddrListen;
	std::string clientIP;
	std::string clientMachine;
	std::string clientUserName;
	std::string clientDomain;
	std::thread lastActivity;
	std::thread sendMsg;
	std::thread screenSend;
	std::ostringstream currentTimeOSS;
	std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
};


// TODO: установите здесь ссылки на дополнительные заголовки, требующиеся для программы.
