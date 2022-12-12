
#include "ws2tcpip.h"
#include "stdio.h";
#include "stdlib.h";

typedef struct filePartData {
	int idFile;
	sockaddr_in ipClientSocket; 	//there is port
	char* filePartAddress;
	int filePartSize;
	//filePartData* nextPart;
}filePartData;

typedef struct hashValue {
	char** completeFile;
	filePartData* filePartDataList;
}hashValue;


