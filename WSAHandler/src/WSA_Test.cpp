#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <thread> //Listener on tcp irc in its own thread.
#include <mutex>
#include <iostream>
#include <cmath>

//Handles the TCP connection
#include "WSAHandler.h"

//Structs
#include "MessageIn.h"
#include "MessageOut.h"

/* Your engine goes here for example.
internal Engine eng;
*/

int main(int argc, char* argv[])
{
	WSAHandler wsah;
	if (!wsah.createAndConnect()){
		printf("failed connecting.\n");
	}
	printf("connected...\n");
	std::mutex miqMutex;
    std::mutex moqMutex;
    std::vector<MessageIn> miq;
	std::vector<MessageOut> moq;
	wsah.linkMutex(&miqMutex, &moqMutex);
	wsah.linkQueues(&miq, &moq);
	
	/*	Give your engine access to the mutex and queues...
	eng.init();
	eng.linkMutex(&miqMutex, &moqMutex);
	eng.linkQueues(&miq, &moq);
	*/

	//listens and fills message in Queue of wsah with incoming messages
	std::thread listener (&WSAHandler::startListening, wsah);
	
	//Test message added to message out queue to be displayed later. Normaly your engine would create these.
	MessageOut mo1;
	mo1.target = "all";
	mo1.message = "Hello World, I am functioning correctly!";
	wsah.addMessageOut(mo1);

	bool EngineIsRunning = true;
	while(EngineIsRunning){
		
		/* 	Test console display of incoming messages.
			Normaly you would want your engine to handle this instead. */
		MessageIn currentMSG = wsah.getNextMessage();
		while(currentMSG.origin != ""){
			std::cout 	<< currentMSG.origin << " sent the message: " 
						<< currentMSG.message << " with WhisperFlag = " 
						<< currentMSG.WhisperFlag <<"\n";
			//Basic logic to test if whispers work.
			if(currentMSG.message == "!whispertest"){
				MessageOut mo2;
				mo2.target = currentMSG.origin;
				mo2.message = "I send this whisper to you. Whisper me back and check the WhisperFlag in your console.";
				wsah.addMessageOut(mo2);
			}	
			currentMSG = wsah.getNextMessage();
		}

		//At this point we will send all outgoing messages that are queued.
		wsah.sendMessages();

		/*
		//Update yuor engine in this loop
		eng.handleMessages();
		eng.update(DeltaTime);
		//Render Scene
		eng.render();
		*/
	}
	/* Dont forget to clean up your engine.
	eng.cleanUp();
	*/
	wsah.cleanUp();

	printf("disconnected, closing down...\n");
	return 0;
}

