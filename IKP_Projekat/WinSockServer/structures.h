#pragma once
#include "ws2def.h";
typedef struct hashValue {
	char** completeFile;
	filePartData* filePartDataList;
}hashValue;

typedef struct filePartData {
	int idFile;
	sockaddr_in ipClientSocket; 	//there is port
	char* filePartAddress;
	int filePartSize;
	//filePartData* nextPart;
}filePartData;

