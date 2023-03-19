#pragma once
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct threadNode {
	HANDLE thread;
	DWORD threadId;
	struct threadNode* next;
}threadNode;

void printList(threadNode* head);

threadNode* createNewThreadNode(HANDLE thread, DWORD threadId);

//brisanje prvog elementa liste
//prosledi se head, 
threadNode* deleteFirstthreadNode(threadNode* head);

//ovo je brisanje preko reference
void deleteFirstthreadNodeBYReference(threadNode** head);

threadNode* insertAtHead(threadNode** head, threadNode* threadNodeToInsert);

threadNode* insertAtEnd(threadNode** head, threadNode* threadNodeToInsert);

threadNode* findthreadNodeByThreadId(threadNode* head, DWORD threadId);

void printFoundthreadNode(threadNode* head, DWORD threadId);

void deleteLastthreadNode(threadNode* head);

void deletethreadNode(struct threadNode** head, DWORD threadId);
