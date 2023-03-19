#include "filePartDataList.h";

filePartDataNode* createNewFilePartDataNode(int relativeAddress, int filePartSize) {
	filePartDataNode* newFilePartDataNode = (filePartDataNode*)malloc(sizeof(filePartDataNode));
	newFilePartDataNode->filePartAddress = (char*)malloc(sizeof(char) * filePartSize);
	newFilePartDataNode->relativeAddress = relativeAddress;
	newFilePartDataNode->filePartSize = filePartSize;
	newFilePartDataNode->next = NULL;
	return newFilePartDataNode;
}

filePartDataNode* insertAtHead(filePartDataNode** head, filePartDataNode* filePartDataNodeToInsert) {
	//kazemo da pokazivac u elementu koji zelimo da ubacimo treba na head da pokazuje
	//na taj nacin se desava da elementi liste budu: 
	//        threadNodeToInsert -> head -> elem1 -> elem2 -> ... -> NULL
	filePartDataNodeToInsert->next = *head;

	//potom, head stavimo da bude taj element koji smo ubacili
	//pa se onda head postavi na threadNode to insert
	// head(threadNodeToInsert) -> head(malo pre ovde bio) -> elem1 -> elem2 -> ... -> NULL
	*head = filePartDataNodeToInsert;

	return filePartDataNodeToInsert;
}

void deleteFilePartDataNode(struct filePartDataNode** head, int relativeAddress)
{
	// Store head threadNode
	struct filePartDataNode* temp = *head, *prev = NULL;

	// If head threadNode itself holds the key to be deleted
	if (temp != NULL && temp->relativeAddress == relativeAddress) {
		*head = temp->next; // Changed head
		free(temp); // free old head
		return;
	}

	// Search for the key to be deleted, keep track of the
	// previous threadNode as we need to change 'prev->next'
	while (temp != NULL && temp->relativeAddress != relativeAddress) {
		prev = temp;
		temp = temp->next;
	}

	// If key was not present in linked list
	if (temp == NULL)
		return;

	// Unlink the threadNode from linked list
	prev->next = temp->next;

	free(temp); // Free memory
}

void bubbleSort(struct filePartDataNode* start)
{
	int swapped, i;
	struct filePartDataNode* ptr1;
	struct filePartDataNode* lptr = NULL;

	/* Checking for empty list */
	if (start == NULL)
		return;

	do
	{
		swapped = 0;
		ptr1 = start;

		while (ptr1->next != lptr)
		{
			if (ptr1->relativeAddress > ptr1->next->relativeAddress)
			{
				swap(ptr1, ptr1->next);
				swapped = 1;
			}
			ptr1 = ptr1->next;
		}
		lptr = ptr1;
	} while (swapped);
}

/* function to swap data of two nodes a and b*/
void swap(struct filePartDataNode* a, struct filePartDataNode* b)
{
	int tempRelativeAddress = a->relativeAddress;
	int tempFilePartSize = a->filePartSize;
	char* tempFilePartAddress = a->filePartAddress;
	a->relativeAddress = b->relativeAddress;
	a->filePartSize = b->filePartSize;
	a->filePartAddress = b->filePartAddress;
	b->relativeAddress = tempRelativeAddress;
	b->filePartSize = tempFilePartSize;
	b->filePartAddress = tempFilePartAddress;
}

void deleteList(struct filePartDataNode** head_ref)
{
	/* deref head_ref to get the real head */
	struct filePartDataNode* current = *head_ref;
	struct filePartDataNode* next;

	while (current != NULL)
	{
		next = current->next;
		free(current);
		current = next;
	}

	/* deref head_ref to affect the real head back
	   in the caller. */
	*head_ref = NULL;
}