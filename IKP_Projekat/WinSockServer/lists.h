#pragma once
#include "structures.h";
#include "string.h";

// Define the Hash map Item here
typedef struct hmItem {
    int key; //id fajla
    hashValue* value;
}hmItem;

// Define the Linkedlist here
typedef struct colisionHmItem {
    hmItem* item;
    colisionHmItem* next;
}colisionHmItem;

colisionHmItem* allocateCollisionHmItem();

colisionHmItem* collisionListInsert(colisionHmItem* collisionList, hmItem* hmItem);

hmItem* collisionHmElementsListRemove(colisionHmItem* head);

void freeCollisionHmElementsList(colisionHmItem* head);


//ovo dalje je iz filePartDataList
void printList(filePartData* head);

filePartData* createNewFilePartData(int idFile, sockaddr_in ipClientSocket,
	char* filePartAddress, int filePartSize);

filePartData* insertAtHead(filePartData** head, filePartData* filePartDataToInsert);

filePartData* insertAtEnd(filePartData** head, filePartData* filePartDataToInsert);

filePartData* findFilePartDataByClientSocket(filePartData* head, sockaddr_in ipClientSocket);

void printFoundFilePartData(filePartData* head, sockaddr_in ipClientSocket);

void deleteFilePartDataLogical(filePartData** head, sockaddr_in ipClientSocket);

int updateFilePartData(filePartData** head, sockaddr_in ipClientSocket);

int filePartDataCount(filePartData* head);

void deleteLastFilePartData(filePartData* head);

//brisanje prvog elementa liste
//prosledi se head, 
//filePartData* deleteFirstFilePartData(filePartData* head);

//ovo je brisanje preko reference
//void deleteFirstFilePartDataBYReference(filePartData** head);

//void deleteLastFilePartData(filePartData* head);