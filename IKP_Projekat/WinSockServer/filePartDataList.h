#pragma once
#include "structures.h";

void printList(filePartData* head);

filePartData* createNewFilePartData(HANDLE thread, DWORD threadId);

//brisanje prvog elementa liste
//prosledi se head, 
filePartData* deleteFirstFilePartData(filePartData* head);

//ovo je brisanje preko reference
void deleteFirstFilePartDataBYReference(filePartData** head);

filePartData* insertAtHead(filePartData** head, filePartData* filePartDataToInsert);

filePartData* insertAtEnd(filePartData** head, filePartData* filePartDataToInsert);

filePartData* findFilePartDataByFileId(filePartData* head, DWORD threadId);

void printFoundFilePartData(filePartData* head, DWORD threadId);

void deleteLastFilePartData(filePartData* head);

void deletefilePartData(struct filePartData** head, DWORD threadId);





