#include "lists.h";

colisionHmItem* allocateCollisionHmItem() {
    // Allocates memory for a Linkedlist pointer
    colisionHmItem* collisionHmItem = (colisionHmItem*)malloc(sizeof(colisionHmItem));
    return collisionHmItem;
}

colisionHmItem* collisionListInsert(colisionHmItem* collisionList, hmItem* hmItem) {
    // Inserts the item onto the Linked List
    if (!collisionList) {
        colisionHmItem* head = allocateCollisionHmItem();
        head->item = hmItem;
        head->next = NULL;
        collisionList = head;
        return collisionList;
    }
    else if (collisionList->next == NULL) {
        colisionHmItem* node = allocateCollisionHmItem();
        node->item = hmItem;
        node->next = NULL;
        collisionList->next = node;
        return collisionList;
    }

    colisionHmItem* temp = collisionList;
    while (temp->next) {
        temp = temp->next;
    }

    //temp je predposlednji
    colisionHmItem* node = allocateCollisionHmItem();
    node->item = hmItem;
    node->next = NULL;
    temp->next = node;

    return collisionList;
}

hmItem* collisionHmElementsListRemove(colisionHmItem* head) {
    // Removes the head from the linked list
    // and returns the item of the popped element
    if (!head)
        return NULL;
    if (!head->next)
        return NULL;
    colisionHmItem* node = head->next;
    colisionHmItem* temp = head;
    temp->next = NULL;
    head = node;
    hmItem* it = NULL;
    memcpy(temp->item, it, sizeof(hmItem));
    //free(temp->item->key);
    free(temp->item->value);
    free(temp->item);
    free(temp);
    return it;
}

void freeCollisionHmElementsList(colisionHmItem* head) {
    colisionHmItem* temp = head;
    while (head) {
        temp = head;
        head = head->next;
        //free(temp->item->key);
        free(temp->item->value);
        free(temp->item);
        free(temp);
    }
}

