#pragma once
#include "structures.h";
#include "string.h";

// Define the Hash map Item here
typedef struct hmItem {
    int key;
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