
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
filePartData* createNewFilePartData(int idFile, sockaddr_in ipClientSocket, char* filePartAddress, int filePartSize) {
	filePartData* newNode = (filePartData*)malloc(sizeof(filePartData));
	newNode->filePartAddress = (char*)malloc(sizeof(char) * filePartSize);
	newNode->idFile = idFile;
	newNode->filePartSize = filePartSize;
	newNode->ipClientSocket = ipClientSocket;
	memcpy(newNode->filePartAddress, filePartAddress, filePartSize);
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
	filePartData* temp = *head;

	if (temp == NULL) {
		return;
	}
	while (temp != NULL) {
		if (temp->ipClientSocket.sin_addr.S_un.S_addr == ipClientSocket.sin_addr.S_un.S_addr
			&& temp->ipClientSocket.sin_port == ipClientSocket.sin_port) {
				temp->ipClientSocket.sin_port = htons(0);
				temp->ipClientSocket.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
				return;
		}
		temp = temp->nextPart;
	}

	return;
}

int updateFilePartData(filePartData** head, sockaddr_in ipClientSocket) {
	filePartData* temp = *head;
	
	while (temp != NULL) {
		if (temp->ipClientSocket.sin_addr.S_un.S_addr == inet_addr("0.0.0.0") &&
			temp->ipClientSocket.sin_port == htons(0)) {
			temp->ipClientSocket = ipClientSocket;
			return 0;
		}
		temp = temp->nextPart;
	}
	return 1;
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

void deleteLastFilePartData(filePartData* head) {
	filePartData* temp = head;
	filePartData* prev;
	//trazimo threadNode koji je pretposlednji
	while (temp->nextPart->nextPart != NULL) {
		temp = temp->nextPart;
	}

	//posle ovog loop-a, u tempu nam se nalazi pretposlednji threadNode, sto znaci da je temp->next poslednji
	//i mi taj poslednji nod stavimo u neku promenjivu, temp->next postavimo na NULL, pa on sad postaje zadnji threadNode
	filePartData* filePartDataToDelete = temp->nextPart;
	temp->nextPart = NULL;

	//threadNode koji smo obrisali moramo da mu oslobodimo memoriju
	free(filePartDataToDelete);
}


