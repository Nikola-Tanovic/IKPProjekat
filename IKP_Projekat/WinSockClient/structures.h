#pragma once

#include "ws2tcpip.h"
#include "stdio.h";
#include "stdlib.h";

typedef struct filePartData {
	int idFile;
	sockaddr_in ipClientSocket; 	//there is port
	char* filePartAddress;
	int filePartSize;
	filePartData* nextPart;
}filePartData;

typedef struct hashValue {
	//nije string, nego pokazivac na string zbog memorije
	char** completeFile;
	filePartData* filePartDataList;
}hashValue;


typedef struct request {
	long fileId;
	long bufferSize;
}request;

typedef struct filePartDataResponse {
	sockaddr_in ipClientSocket; 	//there is port
	int filePartSize;
	int relativeAddress;
	char* filePartAddress;
};

typedef struct fileDataResponse {
	short responseSize;
	long partsCount;
	filePartDataResponse* filePartData; //niz fajlPartData
}fileDataResponse;

typedef struct fileDataResponseSerialized {
	short responseSize;
	long partsCount;
	char* filePartData;
}fileDataResponseSerialized;