#pragma once;
#include "hashMap.h";

hmItem* createItem(int key, hashValue* value) {
    // Creates a pointer to a new hash table item
    hmItem* item = (hmItem*)malloc(sizeof(hmItem));
    item->key = (int)malloc(sizeof(int));
    item->value = (hashValue*)malloc(sizeof(hashValue));

    item->key = key;
    memcpy(item->value, value, sizeof(hashValue));

    return item;
}

void freeItem(hmItem* item) {
    // Frees an item
    //free(item->key);
    free(item->value);
    free(item);
}

int hashFunction(int key)
{
	return (key % MAP_SIZE);
}

void handleCollision(hashMap* map, int index, hmItem* item) {
    colisionHmItem* head = map->collisionLists[index];

    if (head == NULL) {
        // We need to create the list
        head = allocateCollisionHmItem();
        head->item = item;
        head->next = NULL;
        map->collisionLists[index] = head;
        return;
    }
    else {
        // Insert to the list
        map->collisionLists[index] = collisionListInsert(head, item);
        return;
    }
}

void hmInsert(hashMap* map, int key, hashValue* value) {
    // Create the item
    hmItem* item = createItem(key, value);

    int index = hashFunction(key);

    hmItem* currentItem = map->items[index];

    if (currentItem == NULL) {
        // Key does not exist.
        if (map->count == map->size) {
            // Hash map Full
            printf("Insert Error: Hash map is full\n");
            return;
        }

        // Insert directly

        
        //*(map->items + index) = item;
        map->items[index] = item;
        map->count++;
    }
    else {
        // Scenario 1: We only need to update value
        if (currentItem->key == key) {
            //strcpy(table->items[index]->value, value);
            /*
            * ovde cemo dobiti pokazivac na pocetak liste filePartDataList* head = table->items[index]->value->filePartDataList 
            * i onda cemo u tu listu ubacivati podatke o klijentu koji ima kod sebe deo tog fajla(upisujemo tek kad dobijemo potvrdu od klijenta
            * da je kod sebe uspesno sacuvao deo fajla)
            */
            return;
        }
        else {
            // Scenario 2: Collision
            // We will handle case this a bit later
            handleCollision(map, index, item);
            return;
        }
    }
}

hashValue* hmSearch(hashMap* map, int key) {
    // Searches the key in the hash map
    // and returns NULL if it doesn't exist
    int index = hashFunction(key);
    hmItem* item = map->items[index];
    colisionHmItem* head = map->collisionLists[index];

    // Ensure that we move to a non NULL item
    while (item != NULL) {
        if (item->key == key)
            return item->value;
        if (head == NULL)
            return NULL;
        item = head->item;
        head = head->next;
    }
    return NULL;
}

colisionHmItem** createCollisionList(hashMap* hashMap) {
    // Create the overflow buckets; an array of linkedlists
    colisionHmItem** collisionItems = (colisionHmItem**)calloc(hashMap->size, sizeof(colisionHmItem*));
    for (int i = 0; i < hashMap->size; i++)
        collisionItems[i] = NULL;
    return collisionItems;
}

void freeCollisionList(hashMap* hashMap) {
    // Free all the overflow bucket lists
    colisionHmItem** collisionItems = hashMap->collisionLists;
    for (int i = 0; i < hashMap->size; i++)
       freeCollisionHmElementsList(collisionItems[i]);
    free(collisionItems);
}

void freeMap(hashMap* map) {
    // Frees the map
    for (int i = 0; i < map->size; i++) {
        hmItem* item = map->items[i];
        if (item != NULL)
            freeItem(item);
    }

    freeCollisionList(map);
    free(map->items);
    free(map);
}

hashMap* createMap(int size) {
    // Creates a new HashTable
    hashMap* map = (hashMap*)malloc(sizeof(hashMap));
    map->size = size;
    map->count = 0;
    map->items = (hmItem**)calloc(map->size, sizeof(hmItem*));
    for (int i = 0; i < map->size; i++)
        map->items[i] = NULL;
    map->collisionLists = createCollisionList(map);

    return map;
}


