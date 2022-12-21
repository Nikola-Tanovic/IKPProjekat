
#include "lists.h";

/*
typedef struct filePartData {
	int idFile;
	sockaddr_in ipClientSocket; 	//there is port
	char* filePartAddress;
	int filePartSize;
	filePartData* nextPart;
}filePartData;*/

void printList(filePartData* head) {
	filePartData* temp = head;
	while (temp != NULL) {
		printf("\nFile Id: %d", temp->idFile);
		printf("\nIP Client Address: %d", temp->ipClientSocket.sin_addr);
		printf("\nClient Port: %d", temp->ipClientSocket.sin_port);
		printf("\nClient part of file: %s", temp->filePartAddress);
		printf("\nClient file part size: %d", temp->filePartSize);
		printf("\n");
		temp = temp->nextPart;
	}
	
}

//ne zaboraviti da ovde prilikom postavljanja ipClientSocket.port kazemo htons
filePartData* createNewFilePartData(int idFile, sockaddr_in ipClientSocket,
	char* filePartAddress, int filePartSize) {
	filePartData* newNode = (filePartData*)malloc(sizeof(filePartData));
	newNode->filePartAddress = filePartAddress; //jel moze ovo ovako
	newNode->idFile = idFile;
	newNode->filePartSize = filePartSize;
	newNode->ipClientSocket = ipClientSocket;
	newNode->nextPart = NULL;
	return newNode;
}

filePartData* insertAtHead(filePartData** head, filePartData* filePartDataToInsert) {
	filePartDataToInsert->nextPart = *head;
	*head = filePartDataToInsert;
	return filePartDataToInsert;
}

filePartData* insertAtEnd(filePartData** head, filePartData* filePartDataToInsert) {
	filePartData* temp = *head;

	if (*head == NULL) {
		*head = filePartDataToInsert;
		return filePartDataToInsert;
	}

	while (temp->nextPart != NULL) {
		temp = temp->nextPart;
	}

	temp->nextPart = filePartDataToInsert;
	return filePartDataToInsert;

}

filePartData* findFilePartDataByClientSocket(filePartData* head, sockaddr_in ipClientSocket) {
	filePartData* temp = head;
	if (temp == NULL) {
		return NULL;
	}

	while (temp != NULL) {
		if (temp->ipClientSocket.sin_addr.S_un.S_addr == ipClientSocket.sin_addr.S_un.S_addr
			&& temp->ipClientSocket.sin_port == ipClientSocket.sin_port) {
			return temp;
		}
		temp = temp->nextPart;
	}
	return temp;
}

void printFoundFilePartData(filePartData* head, sockaddr_in ipClientSocket) {
	filePartData* foundFilePartData = findFilePartDataByClientSocket(head, ipClientSocket);
	if (foundFilePartData == NULL) {
		printf("\nElement nije pronadjen");
	}
	else {
		printf("\nDeo fajla je pronadjen");
		printf("\nFile Id: %d", foundFilePartData->idFile);
		printf("\nIP Client Address: %d", foundFilePartData->ipClientSocket.sin_addr);
		printf("\nClient Port: %d", foundFilePartData->ipClientSocket.sin_port);
		printf("\nClient part of file: %s", foundFilePartData->filePartAddress);
		printf("\nClient file part size: %d", foundFilePartData->filePartSize);
		printf("\n");
	}



}

void deleteFilePartDataLogical(filePartData** head, sockaddr_in ipClientSocket) {
	filePartData* temp = *head, * prev = NULL;

	if (temp != NULL && temp->ipClientSocket.sin_addr.S_un.S_addr == ipClientSocket.sin_addr.S_un.S_addr
		&& temp->ipClientSocket.sin_port == ipClientSocket.sin_port) {
		temp->ipClientSocket.sin_port = htons(0);
		temp->ipClientSocket.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
		return;
	}

	while (temp != NULL && temp->ipClientSocket.sin_addr.S_un.S_addr == ipClientSocket.sin_addr.S_un.S_addr
		&& temp->ipClientSocket.sin_port == ipClientSocket.sin_port) {
		prev = temp;
		temp = temp->nextPart;
	}

	if (temp == NULL) {
		return;
	}
	temp->ipClientSocket.sin_port = htons(0);
	temp->ipClientSocket.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
	
	return;
}

void updateFilePartData(filePartData** head, sockaddr_in ipClientSocket) {
	filePartData* temp = *head, * prev = NULL;

	if (temp != NULL && temp->ipClientSocket.sin_addr.S_un.S_addr == ipClientSocket.sin_addr.S_un.S_addr
		&& temp->ipClientSocket.sin_port == ipClientSocket.sin_port) {
		temp->ipClientSocket.sin_port = ipClientSocket.sin_port;
		temp->ipClientSocket.sin_addr = ipClientSocket.sin_addr;
		return;
	}

	while (temp != NULL && temp->ipClientSocket.sin_addr.S_un.S_addr == ipClientSocket.sin_addr.S_un.S_addr
		&& temp->ipClientSocket.sin_port == ipClientSocket.sin_port) {
		prev = temp;
		temp = temp->nextPart;
	}

	if (temp == NULL) {
		return;
	}
	temp->ipClientSocket.sin_port = ipClientSocket.sin_port;
	temp->ipClientSocket.sin_addr = ipClientSocket.sin_addr;

	return;
}


int filePartDataCount(filePartData* head) {
	int count = 0;
	filePartData* temp = head;
	while (temp != NULL) {
		temp = temp->nextPart;
		count++;
	}
	return count;
}


