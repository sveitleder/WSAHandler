#ifndef MESSAGEOUT_H
#define MESSAGEOUT_H

#include <string>

struct MessageOut{
	std::string target; //if target == "all" -> sends to general chat instead of whisper.
	std::string message;
};

#endif