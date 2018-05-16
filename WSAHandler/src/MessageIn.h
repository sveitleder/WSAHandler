#ifndef MESSAGEIN_H
#define MESSAGEIN_H

#include <string>

struct MessageIn{
	std::string origin;
	std::string message;
	bool WhisperFlag;
};

#endif