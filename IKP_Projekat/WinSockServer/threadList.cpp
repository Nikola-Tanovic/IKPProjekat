#include "threadList.h";

void printList(threadNode* head) {
	threadNode* temp = head;
	while (temp != NULL) {
		printf("%d - ", temp->threadId);
		temp = temp->next;
	}
	printf("\n");
}

threadNode* createNewThreadNode(HANDLE thread, DWORD threadId) {
	threadNode* newthreadNode = (threadNode*)malloc(sizeof(threadNode));
	newthreadNode->thread = thread;
	newthreadNode->threadId = threadId;
	newthreadNode->next = NULL;
	return newthreadNode;
}

threadNode* deleteFirstthreadNode(threadNode* head) {
	threadNode* temp = head;

	//kada se ovo uradi, prvi element liste ce biti smesten u temp, jer je sad head ustvari head->next 
	head = head->next;

	//mora da oslobodimo memoriju tog elementa
	free(temp);

	//vratimo novi head
	return head;
}

void deleteFirstthreadNodeBYReference(threadNode** head) {
	(*head) = (*head)->next;
}

threadNode* insertAtHead(threadNode** head, threadNode* threadNodeToInsert) {
	//kazemo da pokazivac u elementu koji zelimo da ubacimo treba na head da pokazuje
	//na taj nacin se desava da elementi liste budu: 
	//        threadNodeToInsert -> head -> elem1 -> elem2 -> ... -> NULL
	threadNodeToInsert->next = *head;

	//potom, head stavimo da bude taj element koji smo ubacili
	//pa se onda head postavi na threadNode to insert
	// head(threadNodeToInsert) -> head(malo pre ovde bio) -> elem1 -> elem2 -> ... -> NULL
	*head = threadNodeToInsert;

	return threadNodeToInsert;
}


threadNode* insertAtEnd(threadNode** head, threadNode* threadNodeToInsert) {
	threadNode* temp = *head;

	if (*head == NULL) {
		*head = threadNodeToInsert;
		return threadNodeToInsert;
	}

	//na ovaj nacin dolazimo do poslednjeg cvora
	//jer ako je temp poslednji cvor, onda ce temp->next da pokazuje na NULL
	while (temp->next != NULL) {
		temp = temp->next;
	}

	//sad je temp poslednji cvor
	temp->next = threadNodeToInsert;

	return threadNodeToInsert;
}

threadNode* findthreadNodeByThreadId(threadNode* head, DWORD threadId) {
	threadNode* temp = head;

	//provera da li je head == NULL tj. da li ima elemenata u listi
	if (temp == NULL) {
		return NULL;
	}
	while (temp != NULL) {
		//ako nadjemo element, uradimo return;
		if (temp->threadId == threadId) {
			return temp;
		}
		temp = temp->next;
	}
	return temp;
}

void printFoundthreadNode(threadNode* head, DWORD threadId) {
	threadNode* findthreadNode = findthreadNodeByThreadId(head, threadId);
	if (findthreadNode == NULL) {
		printf("Element nije pronadjen\n");
	}
	else {
		printf("threadNode found! \nthreadNode adress: %p\n Thread ID: %d\n", findthreadNode, findthreadNode->threadId);
	}
}


void deleteLastthreadNode(threadNode* head) {
	threadNode* temp = head;
	threadNode* prev;
	//trazimo threadNode koji je pretposlednji
	while (temp->next->next != NULL) {
		temp = temp->next;
	}

	//posle ovog loop-a, u tempu nam se nalazi pretposlednji threadNode, sto znaci da je temp->next poslednji
	//i mi taj poslednji nod stavimo u neku promenjivu, temp->next postavimo na NULL, pa on sad postaje zadnji threadNode
	threadNode* threadNodeToDelete = temp->next;
	temp->next = NULL;

	//threadNode koji smo obrisali moramo da mu oslobodimo memoriju
	free(threadNodeToDelete);
}

void deletethreadNode(struct threadNode** head, DWORD threadId)
{
	// Store head threadNode
	struct threadNode* temp = *head, * prev = NULL;

	// If head threadNode itself holds the key to be deleted
	if (temp != NULL && temp->threadId == threadId) {
		*head = temp->next; // Changed head
		free(temp); // free old head
		return;
	}

	// Search for the key to be deleted, keep track of the
	// previous threadNode as we need to change 'prev->next'
	while (temp != NULL && temp->threadId != threadId) {
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

