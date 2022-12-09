#pragma once
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct node {
	HANDLE thread;
	DWORD threadId;
	struct node* next;
}node;

void printList(node* head);

node* createNewNode(HANDLE thread, DWORD threadId);

//brisanje prvog elementa liste
//prosledi se head, 
node* deleteFirstNode(node* head);

//ovo je brisanje preko reference
void deleteFirstNodeBYReference(node** head);

node* insertAtHead(node** head, node* nodeToInsert);

node* insertAtEnd(node** head, node* nodeToInsert);

node* findNodeByThreadId(node* head, DWORD threadId);

void printFoundNode(node* head, DWORD threadId);

void deleteLastNode(node* head);

void deleteNode(struct node** head, DWORD threadId);
