#include "threadList.h";

void printList(node* head) {
	node* temp = head;
	while (temp != NULL) {
		printf("%d - ", temp->threadId);
		temp = temp->next;
	}
	printf("\n");
}

node* createNewNode(HANDLE thread, DWORD threadId) {
	node* newNode = (node*)malloc(sizeof(node));
	newNode->thread = thread;
	newNode->threadId = threadId;
	newNode->next = NULL;
	return newNode;
}

node* deleteFirstNode(node* head) {
	node* temp = head;

	//kada se ovo uradi, prvi element liste ce biti smesten u temp, jer je sad head ustvari head->next 
	head = head->next;

	//mora da oslobodimo memoriju tog elementa
	free(temp);

	//vratimo novi head
	return head;
}

void deleteFirstNodeBYReference(node** head) {
	(*head) = (*head)->next;
}

node* insertAtHead(node** head, node* nodeToInsert) {
	//kazemo da pokazivac u elementu koji zelimo da ubacimo treba na head da pokazuje
	//na taj nacin se desava da elementi liste budu: 
	//        nodeToInsert -> head -> elem1 -> elem2 -> ... -> NULL
	nodeToInsert->next = *head;

	//potom, head stavimo da bude taj element koji smo ubacili
	//pa se onda head postavi na node to insert
	// head(nodeToInsert) -> head(malo pre ovde bio) -> elem1 -> elem2 -> ... -> NULL
	*head = nodeToInsert;

	return nodeToInsert;
}


node* insertAtEnd(node** head, node* nodeToInsert) {
	node* temp = *head;

	if (*head == NULL) {
		*head = nodeToInsert;
		return nodeToInsert;
	}

	//na ovaj nacin dolazimo do poslednjeg cvora
	//jer ako je temp poslednji cvor, onda ce temp->next da pokazuje na NULL
	while (temp->next != NULL) {
		temp = temp->next;
	}

	//sad je temp poslednji cvor
	temp->next = nodeToInsert;

	return nodeToInsert;
}

node* findNodeByThreadId(node* head, DWORD threadId) {
	node* temp = head;

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

void printFoundNode(node* head, DWORD threadId) {
	node* findNode = findNodeByThreadId(head, threadId);
	if (findNode == NULL) {
		printf("Element nije pronadjen\n");
	}
	else {
		printf("Node found! \nNode adress: %p\n Thread ID: %d\n", findNode, findNode->threadId);
	}
}


void deleteLastNode(node* head) {
	node* temp = head;
	node* prev;
	//trazimo node koji je pretposlednji
	while (temp->next->next != NULL) {
		temp = temp->next;
	}

	//posle ovog loop-a, u tempu nam se nalazi pretposlednji node, sto znaci da je temp->next poslednji
	//i mi taj poslednji nod stavimo u neku promenjivu, temp->next postavimo na NULL, pa on sad postaje zadnji node
	node* nodeToDelete = temp->next;
	temp->next = NULL;

	//node koji smo obrisali moramo da mu oslobodimo memoriju
	free(nodeToDelete);
}

void deleteNode(struct node** head, DWORD threadId)
{
	// Store head node
	struct node* temp = *head, * prev = NULL;

	// If head node itself holds the key to be deleted
	if (temp != NULL && temp->threadId == threadId) {
		*head = temp->next; // Changed head
		free(temp); // free old head
		return;
	}

	// Search for the key to be deleted, keep track of the
	// previous node as we need to change 'prev->next'
	while (temp != NULL && temp->threadId != threadId) {
		prev = temp;
		temp = temp->next;
	}

	// If key was not present in linked list
	if (temp == NULL)
		return;

	// Unlink the node from linked list
	prev->next = temp->next;

	free(temp); // Free memory
}

