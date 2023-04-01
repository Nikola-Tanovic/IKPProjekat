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


filePartDataNode* insertAtHead(filePartDataNode** head, filePartDataNode* filePartDataNodeToInsert);


void deleteFilePartDataNode(struct filePartDataNode** head, int relativeAddress);

void bubbleSort(struct filePartDataNode* start);

void swap(struct filePartDataNode* a, struct filePartDataNode* b);

void deleteList(struct filePartDataNode** head_ref);

