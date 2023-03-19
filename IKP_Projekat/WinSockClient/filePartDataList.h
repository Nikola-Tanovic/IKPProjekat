#pragma once
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct filePartDataNode {
	int relativeAddress;
	int filePartSize;
	char* filePartAddress;
	filePartDataNode* next;
}filePartDataNode;

//void printList(filePartDataNode* head);

filePartDataNode* createNewFilePartDataNode(int relativeAddress, int filePartSize);

//brisanje prvog elementa liste
//prosledi se head, 
//filePartDataNode* deleteFirstFilePartDataNode(filePartDataNode* head);

//ovo je brisanje preko reference
//void deleteFirstFilePartDataNodeBYReference(filePartDataNode** head);

filePartDataNode* insertAtHead(filePartDataNode** head, filePartDataNode* filePartDataNodeToInsert);

//filePartDataNode* insertAtEnd(filePartDataNode** head, filePartDataNode* filePartDataNodeToInsert);

//filePartDataNode* findthreadNodeByThreadId(filePartDataNode* head, int relativeAddress);

//void printFoundFilePartDataNode(filePartDataNode* head, int relativeAddress);

//void deleteLastFilePartDataNode(filePartDataNode* head);

void deleteFilePartDataNode(struct filePartDataNode** head, int relativeAddress);

void bubbleSort(struct filePartDataNode* start);

void swap(struct filePartDataNode* a, struct filePartDataNode* b);

void deleteList(struct filePartDataNode** head_ref);

