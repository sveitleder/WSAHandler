#include "WSAHandler.h"

WSAHandler::WSAHandler(){

}

WSAHandler::~WSAHandler(){

}

MessageIn WSAHandler::getNextMessage(){
	std::lock_guard<std::mutex> lock(*miqMutex);
	if(!miq->empty()){
		MessageIn temp = miq->front();
		miq->erase(miq->begin());
		return temp;
	}else{
		MessageIn end;
		end.origin = "";
		end.message= "";
		end.WhisperFlag = false;
		return end;
	}

}

void WSAHandler::addMessageOut(MessageOut mo){
	std::lock_guard<std::mutex> lock(*moqMutex);
	moq->push_back(mo);
}
		
void WSAHandler::sendMessages(){
	std::lock_guard<std::mutex> lock(*moqMutex);
	while(!moq->empty()){
		MessageOut mo = moq->front();
		moq->erase(moq->begin());
		if(mo.target != "all"){
			sendWhisper(mo.target, mo.message);
			//printf("I sent a whisper.\n");
		}else{
			sendBroadcast(mo.message);
			//printf("I sent a broadcast whisper.\n");
		}
	}
}

void WSAHandler::startListening(){
	while(running){
		char buffer[512], suffix[512];
	  	memset(buffer, 0, sizeof buffer);

		iResult = recv(ConnectSocket, buffer, sizeof buffer, 0);
	    if (iResult > 0){
	    	if (sscanf(buffer, "PING :%s", suffix) > 0){
	      		sprintf(buffer, "PONG :%s\r\n", suffix);
	      		//printf("Pong\n");
	      		send(ConnectSocket, buffer, strlen(buffer), 0);
	      		continue;
	    	}
	    	//printf("found a message\n");
	    	MessageIn mi;
	    	mi.origin = buffer;
	    	//printf(buffer);
	    	mi.message = buffer;
	    	if(initMessagesDone){
	    		//Set Whisper flag and trim message.
	    		int nameEnd = mi.origin.find_first_of('!');
	    		mi.origin = mi.origin.substr(1, nameEnd-1);
	    		//printf("%zu is begin of: %s", mi.message.find_first_of(':', 1)+1, mi.message.c_str());
	    		int messageBegin = mi.message.find_first_of(':', 1)+1;
	    		int messageEnd = mi.message.length()-2; // -2 to cut off "/r/n" that irc sends.
	    		if(mi.message.substr(0, messageBegin).find("WHISPER") != std::string::npos){
	    			mi.WhisperFlag=true;
	    		}else{
	    			mi.WhisperFlag=false;
	    		}
	    		//std::cout << messageBegin << " " << messageEnd << "\n";
	    		mi.message = mi.message.substr(messageBegin, messageEnd-messageBegin);
	    		//std::cout << mi.message.length() << "\n";
	    		std::lock_guard<std::mutex> lock(*miqMutex);
	    		miq->push_back(mi);
	    	}else if(	mi.message.find(":tmi.twitch.tv CAP * ACK :twitch.tv/commands") 
	    				!= std::string::npos ){
	    		initMessagesDone = true;
	    	}
	    	//printf("size: %zu \n", miq->size());
	    }
	    memset(buffer, 0, sizeof buffer);
	}
}

void WSAHandler::update(std::string& message){
	char buffer[512], suffix[512];
  	memset(buffer, 0, sizeof buffer);

	iResult = recv(ConnectSocket, buffer, sizeof buffer, 0);
    if (iResult > 0){
    	if (sscanf(buffer, "PING :%s", suffix) > 0){
      		sprintf(buffer, "PONG :%s\r\n", suffix);
      		send(ConnectSocket, buffer, strlen(buffer), 0);
    	}
    	message = buffer;//printf("%s\r\n", buffer);
    }
    memset(buffer, 0, sizeof buffer);
}

bool WSAHandler::sendWhisper(std::string target, std::string message){
	std::string s = "PRIVMSG "+channel+" :/w "+ target + " " + message + "\r\n";
	iResult = send(ConnectSocket, s.c_str(), (int) strlen(s.c_str()), 0);
		if (iResult == SOCKET_ERROR) {
		    printf("send failed: %d\n", WSAGetLastError());
		    closesocket(ConnectSocket);
		    WSACleanup();
		    return 0;
		}	
	//printf("Bytes Sent: %ld\n", iResult);
	return 1;
}

bool WSAHandler::sendBroadcast(std::string message){
	std::string s = "PRIVMSG #sadtwig :" + message + "\r\n";
	iResult = send(ConnectSocket, s.c_str(), (int) strlen(s.c_str()), 0);
		if (iResult == SOCKET_ERROR) {
		    printf("send failed: %d\n", WSAGetLastError());
		    closesocket(ConnectSocket);
		    WSACleanup();
		    return 0;
		}	
	//printf("Bytes Sent: %ld\n", iResult);
	return 1;
}

bool WSAHandler::listenUntil(std::string& message){
	message = "";
	// IRC doesn't tell us how big the incoming packet is, we just grab data until we see a "\r\n"
	while (activeSocket())
	{	
		char letter;
		int result = recv(ConnectSocket, &letter, 1, 0);

		if (result == SOCKET_ERROR)
		{	
			int error_code = 0;
			int error_code_size = sizeof(error_code);
			getsockopt(ConnectSocket, SOL_SOCKET, SO_ERROR, (char*)&error_code, &error_code_size);

			// Means that recv timed out but there was no error, which is fine.
			if (!error_code){
				return true;
			}
			else
			{
				printf("Socket Error");
				cleanUp();
				return false;
			}
		}
		else
			message.push_back(letter);

		if (message.size() > 1 && message[message.size() - 2] == '\r' &&message[message.size() - 1] == '\n')
			return true;

		// Would never get this big. If this has happened, shut it down.
		if (message.size() > 5000){
			printf("message too long");
			cleanUp();
			return false;
		}
	}

	return false;
}

bool WSAHandler::createAndConnect(){
	running=true;
	initMessagesDone = false;
	readConnectionInfoFromFile();

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
	    printf("WSAStartup failed: %d\n", iResult);
	    return 1;
	}

	//Declare addrinfo
	struct addrinfo *result = NULL,
                *ptr = NULL,
                hints;

	ZeroMemory( &hints, sizeof(hints) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(ip.c_str(), port.c_str(), &hints, &result);
	if (iResult != 0) {
	    printf("getaddrinfo failed: %d\n", iResult);
	    WSACleanup();
	    return 1;
	}

	ConnectSocket = INVALID_SOCKET;
	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr=result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, 
	    ptr->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	//CONNECT

	// Connect to server.
	iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
	    closesocket(ConnectSocket);
	    ConnectSocket = INVALID_SOCKET;
	}
	// Should really try the next address returned by getaddrinfo
	// if the connect call failed
	// But for this simple example we just free the resources
	// returned by getaddrinfo and print an error message

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
	    printf("Unable to connect to server!\n");
	    WSACleanup();
	    return 1;
	}

	//Send Login
	std::string passSend = std::string("PASS " + oauth + "\r\n");
	std::string userSend = std::string("USER " + nick + "\r\n");//+ " 0 * :" + nick + "\r\n");
	std::string nickSend = std::string("NICK " + nick + "\r\n");
	std::string joinSend = std::string("JOIN " + channel + "\r\n");
	std::string wispSend = std::string("CAP REQ :twitch.tv/commands\r\n"); //allows whispers
	

	const char *sendbuf;
	sendbuf = passSend.c_str();

	// Send an initial buffer
	iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
	if (iResult == SOCKET_ERROR) {
	    printf("send failed: %d\n", WSAGetLastError());
	    closesocket(ConnectSocket);
	    WSACleanup();
	    return 1;
	}	
	//printf("Bytes Sent: %ld\n", iResult);

	sendbuf = userSend.c_str();
	iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
		if (iResult == SOCKET_ERROR) {
		    printf("send failed: %d\n", WSAGetLastError());
		    closesocket(ConnectSocket);
		    WSACleanup();
		    return 1;
		}	
	//printf("Bytes Sent: %ld\n", iResult);

	sendbuf = nickSend.c_str();
	iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
	if (iResult == SOCKET_ERROR) {
	    printf("send failed: %d\n", WSAGetLastError());
	    closesocket(ConnectSocket);
	    WSACleanup();
	    return 1; 
	}	
	//printf("Bytes Sent: %ld\n", iResult);
	//*/

	sendbuf = joinSend.c_str();
	iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
	if (iResult == SOCKET_ERROR) {
	    printf("send failed: %d\n", WSAGetLastError());
	    closesocket(ConnectSocket);
	    WSACleanup();
	    return 1;
	}	
	//printf("Bytes Sent: %ld\n", iResult);
	//*/


	sendbuf = wispSend.c_str();
	iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
	if (iResult == SOCKET_ERROR) {
	    printf("send failed: %d\n", WSAGetLastError());
	    closesocket(ConnectSocket);
	    WSACleanup();
	    return 1;
	}	
	//printf("Bytes Sent: %ld\n", iResult);
	//*/
	return true;
}

void WSAHandler::readConnectionInfoFromFile(){
	std::ifstream inFile(CONNECTION_INFO_FILENAME);
	if (!inFile) {
    printf("Unable to open login info file, attempting to create one - find it and enter the necessary info.\n");
    /*nick = DEFAULT_BOT_NAME;
    ip = DEFAULT_SERVER;
    port = DEFAULT_PORT;
    channel = DEFAULT_CHANNEL;
    oauth = DEFAULT_OAUTH;*/
    std::ofstream outfile(CONNECTION_INFO_FILENAME);
    outfile << "BOT_NAME= wsa_testbot\n" << "SERVER_ADRESS= irc.twitch.tv\n" << "SERVER_PORT= 6667\n" << "CHANNEL_NAME= #YourChannelNameGoesHereIncludingTheHashtag\n" <<"OAUTH_TOKEN= oauth:YourOauthKeyGoesHereIfYouChangeTheBotNameToYourOwnBot\n" << std::endl;
    outfile.close();
   	inFile.open(CONNECTION_INFO_FILENAME);
    exit(1);   // call system to stop
	}else{
		std::string temp;
		std::getline(inFile, temp);
		temp.erase(std::remove_if(temp.begin(), temp.end(), [](unsigned char x){return std::isspace(x);}), temp.end()); //sanity check removing excess spaces
		nick = temp.substr(9);
		std::getline(inFile, temp);
		temp.erase(std::remove_if(temp.begin(), temp.end(), [](unsigned char x){return std::isspace(x);}), temp.end()); //sanity check removing excess spaces
		ip = temp.substr(14);
		//std::getline(inFile, ip);
		std::getline(inFile, temp);
		temp.erase(std::remove_if(temp.begin(), temp.end(), [](unsigned char x){return std::isspace(x);}), temp.end()); //sanity check removing excess spaces
		port = temp.substr(12);
		//std::getline(inFile, port);
		std::getline(inFile, temp);
		temp.erase(std::remove_if(temp.begin(), temp.end(), [](unsigned char x){return std::isspace(x);}), temp.end()); //sanity check removing excess spaces
		channel = temp.substr(13);
		//std::getline(inFile, channel);
		std::getline(inFile, temp);
		temp.erase(std::remove_if(temp.begin(), temp.end(), [](unsigned char x){return std::isspace(x);}), temp.end()); //sanity check removing excess spaces
		oauth = temp.substr(12);
		if( nick == DEFAULT_BOT_NAME ){
			oauth = DEFAULT_OAUTH;
			printf("Connecting with \nNick: %s\nIp: %s\nPort: %s\nChannel: %s\nOauth: DEFAULT\n", 
			nick.c_str(), ip.c_str(), port.c_str(), channel.c_str()); //*/
		}else{
			//std::getline(inFile, oauth);
			printf("Connecting with \nNick: %s\nIp: %s\nPort: %s\nChannel: %s\nOauth: %s\n", 
			nick.c_str(), ip.c_str(), port.c_str(), channel.c_str(), oauth.c_str()); //*/
		}
		
	}
	
}

void WSAHandler::linkMutex(std::mutex *miqp, std::mutex *moqp){
	miqMutex = miqp;
	moqMutex = moqp;
}

void WSAHandler::linkQueues(std::vector<MessageIn> *miqp, std::vector<MessageOut> *moqp){
	miq = miqp;
	moq = moqp;
}

bool WSAHandler::disconnect(){
	// shutdown the send half of the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
	    printf("shutdown failed: %d\n", WSAGetLastError());
	    closesocket(ConnectSocket);
	    WSACleanup();
	    return 0;
	}
	return 1;
}

bool WSAHandler::cleanUp(){
	running=false;
	disconnect();
	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();
	return 0;
}