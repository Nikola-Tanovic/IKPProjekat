
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "threadList.h";
#include "string.h";
#include "hashMap.h";


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"
#define FILE_COUNT 10

bool InitializeWindowsSockets();

typedef struct clientCommunicationThreadParameters {
    SOCKET acceptedSocket;
    sockaddr_in clientAddr;
    CRITICAL_SECTION hashMapCS;
    CRITICAL_SECTION threadListCS;
    CRITICAL_SECTION printCS;
    DWORD threadId;
    int* exitFlag;
    threadNode** head;
}clientCommunicationThreadParameters;

typedef struct listenThreadParameters {
    SOCKET listenSocket;
    SOCKET acceptedSocket;
    threadNode** head;
    CRITICAL_SECTION hashMapCS;
    CRITICAL_SECTION threadListCS;
    CRITICAL_SECTION printCS;
    int* exitFlag;
    DWORD threadId;
}listenThreadParameters;

typedef struct errorResponse {
    char* responseMessage;
    long filesCount;
};


DWORD WINAPI clientThreadFunction(LPVOID lpParam);
DWORD WINAPI listenThreadFunction(LPVOID lpParam);

int  main(void)
{
    char* fileArray[] = {
    "Led led sladoled, sladja cura nego med",
    "Skeledzijo na Moravi prijatelju stari, dal si skoro prevezao preko dvoje mladih",
    "Svi kockari gube sve kasnije il pre",
    "Zasto nisam pticica, da letim daleko, pa da vidim napolju, ceka li me neko",
    "Ja necu lepsu, ja necu drugu, birao sam sreco moja ili tebe ili tugu",
    "Ja sam ja, Jeremija, prezivam se Krstic",
    "Iznenada u kafani staroj, sinoc sretoh prijatelja svoga",
    "Ja sam seljak, veseljak, al u dusi tugu skrivam",
    "Tamo gde si ti tamo stanuje mi dusa, ranjena dusa zeljna ljubavi",
    "Hvala ti za ljubav sto od tebe primam, ja sam srecan covek, srecan sto te imam"
    };

    // Socket used for listening for new clients 
    SOCKET listenSocket = INVALID_SOCKET;
    // Socket used for communication with client
    //SOCKET acceptedSocket = INVALID_SOCKET;
    
    // variable used to store function return value
    int iResult;
    // Buffer used for storing incoming data
    char recvbuf[DEFAULT_BUFLEN];

    unsigned long mode = 1;
    int result = 0;
    int exitFlag = 0;

    //creating and initializing critical section 
    CRITICAL_SECTION printCS;
    InitializeCriticalSection(&printCS);

    //critical section that controls access to threadList
    CRITICAL_SECTION threadListCS;
    InitializeCriticalSection(&threadListCS);

    //critical section that controls hash map access
    CRITICAL_SECTION hashMapCS;
    InitializeCriticalSection(&hashMapCS);

    //initializing list of threads
    threadNode* head = NULL;

    hashMap* hashMap = createMap(MAP_SIZE);
    for (int i = 0; i < FILE_COUNT; i++) {
        hashValue* hashValueInsert = (hashValue*)malloc(sizeof(hashValue));
        hashValueInsert->completeFile = &fileArray[i];
        hashValueInsert->filePartDataList = NULL;
        hmInsert(hashMap, i, hashValueInsert);
    }

    
    hashValue* hv = hmSearch(hashMap, 9);



    if (InitializeWindowsSockets() == false)
    {
        // we won't log anything since it will be logged
        // by InitializeWindowsSockets() function
        return 1;
    }

    // Prepare address information structures
    addrinfo* resultingAddress = NULL;
    addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4 address
    hints.ai_socktype = SOCK_STREAM; // Provide reliable data streaming
    hints.ai_protocol = IPPROTO_TCP; // Use TCP protocol
    hints.ai_flags = AI_PASSIVE;     // 

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &resultingAddress);
    if (iResult != 0)
    {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    listenSocket = socket(AF_INET,      // IPv4 address famly
        SOCK_STREAM,  // stream socket
        IPPROTO_TCP); // TCP

    if (listenSocket == INVALID_SOCKET)
    {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(resultingAddress);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket - bind port number and local address 
    // to socket
    iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(resultingAddress);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Since we don't need resultingAddress any more, free it
    freeaddrinfo(resultingAddress);

    // Set listenSocket in listening mode
    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    printf("Server initialized, waiting for clients.\n");

    iResult = ioctlsocket(listenSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);

    
    /// <summary>
    /// pocetak thread-a gde se poziva accept funckija
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    
    listenThreadParameters* ltParams = (listenThreadParameters*)malloc(sizeof(listenThreadParameters));
    ltParams->listenSocket = listenSocket;
    ltParams->head = &head;
    ltParams->hashMapCS = hashMapCS;
    ltParams->threadListCS = threadListCS;
    ltParams->printCS = printCS;
    ltParams->exitFlag = &exitFlag;
    ltParams->threadId = 0;
    ltParams->acceptedSocket = INVALID_SOCKET;

    // listenSocket, head, hashMapCS, threadListCS, printCS

    HANDLE listenThread = CreateThread(NULL, 0, &listenThreadFunction, ltParams, 0, &(ltParams->threadId));
    
    EnterCriticalSection(&threadListCS);
    printf("Thread ID(unutar main funkcije servera, ID listen thread-a): %d\n", ltParams->threadId);
    threadNode* tn = createNewThreadNode(listenThread, ltParams->threadId);
    insertAtHead(&head, tn);
    printList(head);
    printf("Listen thread pokrenut, unesite 'E' za kraj\n");
    LeaveCriticalSection(&threadListCS);

    char input = ' ';
    scanf("%c", &input);
    if (input == 'e') {
        exitFlag = 1;

       /*
       proveravamo da li je lista threadova prazna, ako jeste znaci da su svi threadovi izvrseni i zatvoreni
       zatvaranje threadova radimo u samim thread-ovima tako sto pretrazimo listu threadova po threadId koji imamo u parametrima 
       i onda thread iz tog cvora liste koji smo nasli zatvorimo i nakon toga sam taj node obrisemo iz liste po tom id.
        */
        while(head != NULL) {
            Sleep(200); 
        }
        closesocket(listenSocket);
        WSACleanup();
        free(head);
        freeMap(hashMap);
        return 0;
    }

   
    //promenjiva koja detektuje exit na mainu se prosledjuje lisent thread-u a on prosledjuje thradoivma za komunikaciju sa klijentima
    //taj gethchar stoji u main threadu posle listen thread-a
    //

    // shutdown the connection since we're done
    /*iResult = shutdown(acceptedSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(acceptedSocket);
        WSACleanup();
        return 1;
    }

    // cleanup*/

    
    
    //closesocket(acceptedSocket);

  
    //zatvaramo threadove

    return 0;
}

bool InitializeWindowsSockets()
{
    WSADATA wsaData;
    // Initialize windows sockets library for this process
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed with error: %d\n", WSAGetLastError());
        return false;
    }
    return true;
}

DWORD WINAPI clientThreadFunction(LPVOID lpParam) {

    char recvBuff[DEFAULT_BUFLEN];
    unsigned long mode = 1;
    long fileCount = FILE_COUNT;

    clientCommunicationThreadParameters* tParameters = (clientCommunicationThreadParameters*)lpParam;

    int iResult = ioctlsocket(tParameters->acceptedSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);

    fd_set readfds;

    FD_ZERO(&readfds);

    FD_SET(tParameters->acceptedSocket, &readfds);

    timeval timeVal;
    timeVal.tv_sec = 1;
    timeVal.tv_usec = 0;

    //printf("Konektovan novi klijent\n");

    fileCount = htonl(fileCount);
    //inicijalni send ka klijntu, da bi znao koliko ima fajlova
    iResult = send(tParameters->acceptedSocket, (char*)&fileCount, (int)sizeof(long), 0);

    if (iResult == SOCKET_ERROR)
    {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(tParameters->acceptedSocket);
        WSACleanup();
    }

    EnterCriticalSection(&tParameters->printCS);
    printf("Bytes Sent to client: %ld\n", iResult);
    LeaveCriticalSection(&tParameters->printCS);

    do
    {
        FD_ZERO(&readfds);
        FD_SET(tParameters->acceptedSocket, &readfds);

        int result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
            //Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            printf("Connection with client closed(bababab).\n");
            closesocket(tParameters->acceptedSocket);
            break;
        }

        // Receive data until the client shuts down the connection
        iResult = recv(tParameters->acceptedSocket, recvBuff, DEFAULT_BUFLEN, 0);
        if (iResult > 0)
        {
            recvBuff[iResult] = '\0';
           
          
            request* clientRequest = (request*)recvBuff;
            clientRequest->fileId = ntohl(clientRequest->fileId);
            clientRequest->bufferSize = ntohl(clientRequest->bufferSize);

            EnterCriticalSection(&(tParameters->printCS));
            printf("Client: IPAddress: %s Port: %d\n", inet_ntoa(tParameters->clientAddr.sin_addr), ntohs(tParameters->clientAddr.sin_port));
            printf("Thread ID: %d\n", tParameters->threadId);
            printf("Request from client:\nFile ID: %ld\nClient buff size: %ld\n", clientRequest->fileId, clientRequest->bufferSize);
            FD_CLR(tParameters->acceptedSocket, &readfds);
            LeaveCriticalSection(&(tParameters->printCS));
            
            //citanje fajl i podatke o njegovim delovima iz hash mape
            /*
            * proveravamo da li je lista prazna. Ako jeste, onda imamo prvi ikada zahtev od klijeta,  i onda saljemo ceo fajl
            Ako lista nije prazna, prolazimo kroz nju, i proveravamo za svaki clan liste:
                1) Ako su mu adresa i port na 0, onda samo u odgovoru za klijenta pakujemo deo fajla sa te pocetne adrese i velicine
                koji imamo zapisano u tom cvoru liste(taj cvor liste je u stvari filePartData strucktura)
                2) Ako mu addr i port nisu na 0, onda samo taj filePartData spakujemo u odgovor klijentu(videcemo sta cemo mu slati)

                takodje raditi provere ako klijent posalje fileID koji je veci od broja fajlova, onda posalji gresku klijentu
                i reci koliki broj fajlova postoji.
            */

          


            /*
            kada se klijent diskonektuje, moramo da update hash mapu tj. da sockaddr stavimo na 0
            prvo pretrazimo hash mapu po file id i tako nadjemo taj node u hash mapi. Dobijemo pokazivac na strukturu 
            koja ima pokazivac na ceo fajl i listu filePartData. Prolazimo kroz tu listu i trazimo filePartData koji ima isti
            sockaddr kao i klijent sa kojim komuniciramo na ovom threadu. Tom filePartData stavimo 0 na adresu i port socketa(logicko brisanje) 

            */

            


            //slanje tih podataka ka klijenut

            //cekanje potvrde klijenta da je primio

            //posle potvrde radimo upis u hash mapu


        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
            printf("Connection with client closed.\n");
            closesocket(tParameters->acceptedSocket);
            break;
        }
        else
        {
            // there was an error during recv
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(tParameters->acceptedSocket);
            break;
        }
    } while (!*(tParameters->exitFlag));
    
    //zatvaranje thread-a i close socket 
    EnterCriticalSection(&(tParameters->threadListCS));
    threadNode* nodeToDelete = findthreadNodeByThreadId(*(tParameters->head), tParameters->threadId);
    LeaveCriticalSection(&(tParameters->threadListCS));

    
    //ovde radimo onu logiku za brisanje iz hash mape tj. logicko brisanje
    //treba da se to radi u critical section za menjanje mape. 

    CloseHandle(nodeToDelete->thread);

    EnterCriticalSection(&(tParameters->threadListCS));
    deletethreadNode(tParameters->head, tParameters->threadId);
    printf("Ispis u client communication thread-u\n");
    printList(*(tParameters->head));
    LeaveCriticalSection(&(tParameters->threadListCS));


    return 0;
    // here is where server shutdown loguc could be placed
}

DWORD WINAPI listenThreadFunction(LPVOID lpParam) {
    
    listenThreadParameters* ltParams = (listenThreadParameters*)lpParam;
    fd_set readfds;
    int result = -1;

    do
    {
        FD_ZERO(&readfds);

        FD_SET(ltParams->listenSocket, &readfds);

        timeval timeVal;
        timeVal.tv_sec = 1;
        timeVal.tv_usec = 0;

        result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            //printf("Vreme za cekanje je isteklo (select pre accept funkcije - Server ) \n");
            //Sleep(3000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            printf("Connection with client closed.\n");
            closesocket(ltParams->acceptedSocket);
            break;
        }
        else if (FD_ISSET(ltParams->listenSocket, &readfds)) {
            // Wait for clients and accept client connections.
            // Returning value is acceptedSocket used for further
            // Client<->Server communication. This version of
            // server will handle only one client.

            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(struct sockaddr_in);

            ltParams->acceptedSocket = accept(ltParams->listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);

            if (ltParams->acceptedSocket == INVALID_SOCKET)
            {
                printf("accept failed with error: %d\n", WSAGetLastError());
                closesocket(ltParams->listenSocket);
                WSACleanup();
                break;
            }

            //u tread stavljamo sve ovo ispod ovog komentara, kada prodje accept treba da 
            //napravimo novu nit, u kojoj cemo pozvati ove recieve funkcije

            //cuvamo povratnu vrednost niti
            DWORD threadReturnValue = -1;


            clientCommunicationThreadParameters* tParameters = (clientCommunicationThreadParameters*)malloc(sizeof(clientCommunicationThreadParameters)); //ovde alocirati memoriju mallocom.
            tParameters->acceptedSocket = ltParams->acceptedSocket;
            tParameters->clientAddr = clientAddr;
            tParameters->printCS = ltParams->printCS;
            tParameters->hashMapCS = ltParams->hashMapCS;
            tParameters->threadListCS = ltParams->threadListCS;
            tParameters->threadId = 0;
            tParameters->exitFlag = ltParams->exitFlag;     //umesto return value zvacemo ga exit koji cemo upisivati u main threa-du
            tParameters->head = ltParams->head;             //dodacemo i polje head pokazivac na listu thread=ova koji kominiciraju sa klijentom.

            HANDLE clientCommunicaitonThread = CreateThread(NULL, 0, &clientThreadFunction, tParameters, 0, &(tParameters->threadId));

            //postaviti pitanje vezano za ovo
            //da li moze da se desi da se ne napuni tParameters.threadId a da se udje u ovu kriticnu sekciju?
            EnterCriticalSection(&(ltParams->threadListCS));
            printf("Thread ID(unutar listen thread-a funkcije servera, id thread-a za komunikaciju sa klijentom): %d\n", tParameters->threadId);
            threadNode* tn = createNewThreadNode(clientCommunicaitonThread, tParameters->threadId);
            insertAtHead(ltParams->head, tn);
            printList(*(ltParams->head));
            LeaveCriticalSection(&(ltParams->threadListCS));

            /*
            // postavljanje soketa u neblokirajuci rezim
            iResult = ioctlsocket(acceptedSocket, FIONBIO, &mode);
            if (iResult != NO_ERROR)
                printf("ioctlsocket failed with error: %ld\n", iResult);

            do
            {
                FD_ZERO(&readfds);
                FD_SET(acceptedSocket, &readfds);

                result = select(0, &readfds, NULL, NULL, &timeVal);

                if (result == 0) {
                    printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
                    Sleep(1000);
                    continue;
                }
                else if (result == SOCKET_ERROR) {
                    // connection was closed gracefully
                    printf("Connection with client closed.\n");
                    closesocket(acceptedSocket);
                    break;
                }

                // Receive data until the client shuts down the connection
                iResult = recv(acceptedSocket, recvbuf, DEFAULT_BUFLEN, 0);
                if (iResult > 0)
                {
                    printf("Message received from client: %s.\n", recvbuf);
                    FD_CLR(acceptedSocket, &readfds);
                }
                else if (iResult == 0)
                {
                    // connection was closed gracefully
                    printf("Connection with client closed.\n");
                    closesocket(acceptedSocket);
                }
                else
                {
                    // there was an error during recv
                    printf("recv failed with error: %d\n", WSAGetLastError());
                    closesocket(acceptedSocket);
                }
            } while (iResult > 0);

            // here is where server shutdown loguc could be placed*/

        }

        //gde se vrsi logika zatvaranja soketa i brisanja threadova iz liste

    } while (!*(ltParams->exitFlag));

    //Zatvaranje Handla
    EnterCriticalSection(&(ltParams->threadListCS));
    threadNode* nodeToClose = findthreadNodeByThreadId(*(ltParams->head), ltParams->threadId);
    LeaveCriticalSection(&(ltParams->threadListCS));

    CloseHandle(nodeToClose->thread);
    
    //brisanje thread node iz liste
    EnterCriticalSection(&(ltParams->threadListCS));
    deletethreadNode(ltParams->head, ltParams->threadId);
    printf("Ispis u listen thread-u\n");
    printList(*(ltParams->head));
    LeaveCriticalSection(&(ltParams->threadListCS));

    return 0;
}