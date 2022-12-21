#pragma once
#include "lists.h";

#define MAP_SIZE 3  //size of hash map

// Define the Hash map here
typedef struct hashMap {
    // Contains an array of pointers
    // to items
    hmItem** items;
    colisionHmItem** collisionLists; //lista pokazivaca na collisionHmItem, tj, jedan chain gde imamo koliziju, a node ce biti fajl1 fajl5 fajl7
    int size;
    int count;
}hashMap;

hmItem* createItem(int key, hashValue* value);

void freeItem(hmItem* item);

int hashFunction(int key);

void handleCollision(hashMap* map, int index, hmItem* item); 

void hmInsert(hashMap* map, int key, hashValue* value); 

hashValue* hmSearch(hashMap* map, int key); 

void freeCollisionList(hashMap* hashMap);

void freeMap(hashMap* map);

hashMap* createMap(int size);


