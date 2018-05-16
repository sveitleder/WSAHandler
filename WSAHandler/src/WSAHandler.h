#ifndef WSAHANDLER_H
#define WSAHANDLER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cctype>

#include "MessageOut.h"
#include "MessageIn.h"

#define CONNECTION_INFO_FILENAME "login.ini"

#define DEFAULT_BOT_NAME "wsa_testbot"
#define DEFAULT_SERVER "irc.twitch.tv"
#define DEFAULT_PORT "6667"
#define DEFAULT_CHANNEL "#NoChannel"
#define DEFAULT_OAUTH "oauth:kqfdih5htnjv09lq70s1ax9rpzy76w"

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

class WSAHandler {
	public:
		WSAHandler();
		~WSAHandler();
		//WSAHandler( const WSAHandler& other ) = delete; // non construction-copyable
     	//WSAHandler& operator=( const WSAHandler& ) = delete; //non copyable
		bool cleanUp();
		bool createAndConnect();
		void linkMutex(std::mutex *miqp, std::mutex *moqp);
		void linkQueues(std::vector<MessageIn> *miqp, std::vector<MessageOut> *moqp);
		bool listenUntil(std::string& message);
		bool activeSocket() { return ConnectSocket != NULL; }
		void startListening();
		MessageIn getNextMessage(); //Only used in engine probably
		void addMessageOut(MessageOut mo); //Only used in engine probably
		void sendMessages();
		void update(std::string& message); //unused
	private: 
		std::string nick;
		std::string port;
		std::string ip;
		std::string channel;
		std::string oauth;
		void readConnectionInfoFromFile();
		bool disconnect();
		bool sendWhisper(std::string target, std::string message);
		bool sendBroadcast(std::string message); //sends message to chat/not whisper
		WSADATA wsaData;
		int iResult;
		SOCKET ConnectSocket;
		bool running;
		std::vector<MessageIn> *miq;
		std::vector<MessageOut> *moq;
		std::mutex *miqMutex;
		std::mutex *moqMutex;
		bool initMessagesDone;
};

#include "WSAHandler.cpp"

#endif
