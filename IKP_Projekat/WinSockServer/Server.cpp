
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
    hashMap* hashMap;
}clientCommunicationThreadParameters;

typedef struct listenThreadParameters {
    SOCKET listenSocket;
    SOCKET acceptedSocket;
    threadNode** head; //vrv cu nesto slicno imati na klijentu
    CRITICAL_SECTION hashMapCS;
    CRITICAL_SECTION threadListCS;
    CRITICAL_SECTION printCS;
    int* exitFlag; //vrv cu nesto slicno imati na klijentu
    DWORD threadId;
    hashMap* hashMap;
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
    "Sina majci iz narucja oteo je zulumcar, sada kune svoju sudbu sto je janicar",
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

    //
    //LISTA THREADOVA
    //
    //initializing list of threads
    threadNode* head = NULL;

    hashMap* hashMap = createMap(MAP_SIZE);
    for (int i = 0; i < FILE_COUNT; i++) {
        hashValue* hashValueInsert = (hashValue*)malloc(sizeof(hashValue));
        hashValueInsert->completeFile = &fileArray[i];
        hashValueInsert->filePartDataList = NULL;
        hmInsert(hashMap, i, hashValueInsert);
    }

    
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
        EnterCriticalSection(&printCS);
        printf("getaddrinfo failed with error: %d\n", iResult);
        LeaveCriticalSection(&printCS);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for listening for client sockets
    listenSocket = socket(AF_INET,      // IPv4 address famly
        SOCK_STREAM,  // stream socket
        IPPROTO_TCP); // TCP

    if (listenSocket == INVALID_SOCKET)
    {
        EnterCriticalSection(&printCS);
        printf("socket failed with error: %ld\n", WSAGetLastError());
        LeaveCriticalSection(&printCS);
        freeaddrinfo(resultingAddress);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket - bind port number and local address 
    // to socket
    iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        EnterCriticalSection(&printCS);
        printf("bind failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&printCS);
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
        EnterCriticalSection(&printCS);
        printf("listen failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&printCS);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    EnterCriticalSection(&printCS);
    printf("Server initialized, waiting for clients.\n");
    LeaveCriticalSection(&printCS);

    iResult = ioctlsocket(listenSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
    {
        EnterCriticalSection(&printCS);
        printf("ioctlsocket failed with error: %ld\n", iResult);
        LeaveCriticalSection(&printCS);
    }
    
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
    ltParams->hashMap = hashMap;

    // listenSocket, head, hashMapCS, threadListCS, printCS

    HANDLE listenThread = CreateThread(NULL, 0, &listenThreadFunction, ltParams, 0, &(ltParams->threadId));
    
    EnterCriticalSection(&threadListCS);
    printf("Thread ID(in the main function of server, ID of the listen thread): %d\n", ltParams->threadId);
    threadNode* tn = createNewThreadNode(listenThread, ltParams->threadId);
    insertAtHead(&head, tn);
    printList(head);
    printf("Listen thread started, enter 'e' to exit\n\n");
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
        DeleteCriticalSection(&printCS);
        DeleteCriticalSection(&threadListCS);
        DeleteCriticalSection(&hashMapCS);
        closesocket(listenSocket);
        WSACleanup();
        free(ltParams);
        free(head);
        freeMap(hashMap);
        return 0;
    }

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


    fileCount = htonl(fileCount);

    //initital send to client in order to inform client about how many files there are
    iResult = send(tParameters->acceptedSocket, (char*)&fileCount, (int)sizeof(long), 0);

    if (iResult == SOCKET_ERROR)
    {
        EnterCriticalSection(&tParameters->printCS);
        printf("send failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&(tParameters->printCS));
        closesocket(tParameters->acceptedSocket);
        WSACleanup();

    }

    EnterCriticalSection(&tParameters->printCS);
    printf("Bytes sent to client(informing client of how many files are stored on server): %ld\n\n", iResult);
    LeaveCriticalSection(&tParameters->printCS);

    //UBACIM UMESTO JEDNOG DA SE CUVA LISTA ID-OVA
    long lastRequestedFileId = -1;

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
            EnterCriticalSection(&tParameters->printCS);
            printf("Connection with client closed.\n");
            LeaveCriticalSection(&(tParameters->printCS));
            closesocket(tParameters->acceptedSocket);
            break;
        }

        // Receive data until the client shuts down the connection
        iResult = recv(tParameters->acceptedSocket, recvBuff, DEFAULT_BUFLEN, 0);
        if (iResult > 0)
        {
            char* filePartAddress = NULL;
            int saveAddress = 1;

            recvBuff[iResult] = '\0';
            request* clientRequest = (request*)recvBuff;
            clientRequest->fileId = ntohl(clientRequest->fileId);
            clientRequest->bufferSize = ntohl(clientRequest->bufferSize);

            EnterCriticalSection(&(tParameters->printCS));
            printf("Client IPAddress: %s \nClient Port: %d\n", inet_ntoa(tParameters->clientAddr.sin_addr), ntohs(tParameters->clientAddr.sin_port));
            printf("Thread ID: %d\n", tParameters->threadId);
            printf("\n------------Request from client------------\nFile ID: %ld\nClient buff size: %ld\n-------------------------------------------\n\n", clientRequest->fileId, clientRequest->bufferSize);
            FD_CLR(tParameters->acceptedSocket, &readfds);
            LeaveCriticalSection(&(tParameters->printCS));
            
            //citanje fajl i podatke o njegovim delovima iz hash mape
            /*
            * proveravamo da li je lista prazna. Ako jeste, onda imamo prvi ikada zahtev od klijeta,  i onda saljemo ceo fajl
            Ako lista nije prazna, prolazimo kroz nju, i proveravamo za svaki clan liste:
                1) Ako su mu adresa i port na 0, onda samo u odgovoru za klijenta pakujemo deo fajla sa te pocetne adrese i velicine
                koji imamo zapisano u tom cvoru liste(taj cvor liste je u stvari filePartData strucktura)
                2) Ako mu addr i port nisu na 0, onda samo taj filePartData spakujemo u odgovor klijentu(videcemo sta cemo mu slati)

                Kako pamtimo onaj ostatak od fajla na kraju koji nije ni kod jednog klijenta?
                Tako sto prolazimo kroz listu i detektujemo poslednji clan liste, pamtimo pocetnu adresu tog dela fajla i velicinu
                i onda citamo sa adrese + velicinu iz cistog fajla i taj deo fajla pakujemo za klijenta

                Klijentu cemo slati stukturu podataka koja ce sadrzati 3 polja:
                1) Niz delova fajlova
                2) Niz filePartData
                3) Broj clanova tih nizova (ovo cemo videti koliki ce biti)

                U slucaju da u listi imamo podatke o klijentu, te podatke cemo stavljati u niz filePartData, a u niz delova fajlova cemo 
                stavljati NULL
                U slucaju da u listi nemamo podatke o klijentu, onda cemo u niz filePartData stavljati , dok u niz delova fajlova
                cemo stavljati 


            */

            EnterCriticalSection(&tParameters->hashMapCS);
            hashValue* fileData = hmSearch(tParameters->hashMap, (int)clientRequest->fileId);
            LeaveCriticalSection(&tParameters->hashMapCS);

            fileDataResponse* response = (fileDataResponse*)malloc(sizeof(fileDataResponse));
            response->responseSize = sizeof(fileDataResponse);
            
            response->filePartData = NULL;
            response->partsCount = 0;
            
            int filePartOnClientCounter = 0;
            int summedFilePartSize = 0;
         
            //there is no filePartData that means that nobody has downloaded this file yet
            if (fileData->filePartDataList == NULL) {
                
                response->filePartData = (filePartDataResponse*)malloc(1 * sizeof(filePartDataResponse));
                response->filePartData->filePartAddress = (char*)malloc(sizeof(char) * (strlen(*(fileData->completeFile)) + 1));
                response->responseSize += sizeof(filePartDataResponse) + strlen(*(fileData->completeFile)) + 1;

                //save the start address of the first part of the file which is not stored at any client
                if (saveAddress) {
                    filePartAddress = fileData->completeFile[0];
                    saveAddress = 0;
                }
            
                //this is the very first request of all clients for this file, so we just add data about the client and just send the whole file to the client 
                memcpy(response->filePartData->filePartAddress, fileData->completeFile[0], strlen(fileData->completeFile[0]) + 1);
                response->filePartData->ipClientSocket.sin_port = htons(0); 
                response->filePartData->ipClientSocket.sin_addr.S_un.S_addr = inet_addr("0.0.0.0"); 
                response->filePartData->filePartSize = htonl(strlen(fileData->completeFile[0]) + 1);
                response->filePartData->relativeAddress = htonl(0);


               
                response->partsCount = 1;

                //saving the summed length of all parts of the file which are saved on server and not on any client
                summedFilePartSize += (strlen(*(fileData->completeFile)) + 1);
                
            }
            else {
               /*
                Ako lista nije prazna, prolazimo kroz nju, i proveravamo za svaki clan liste:
                1) Ako su mu adresa i port na 0, onda samo u odgovoru za klijenta pakujemo deo fajla sa te pocetne adrese i velicine
                koji imamo zapisano u tom cvoru liste(taj cvor liste je u stvari filePartData strucktura)
                2) Ako mu addr i port nisu na 0, onda samo taj filePartData spakujemo u odgovor klijentu(videcemo sta cemo mu slati)
                */
                int  i = 0;
                filePartData* iterator = fileData->filePartDataList;
                //nama ce relativna adresa nekog elementa liste biti zbir velicina filePartSize svih prethodnih elemenata list
                int relativeAddress = 0;
                

                //dobijemo broj elemenata filePartData liste i onda odmah zauzmemo memoriju za i+1, 1 zato sto mozda imamo onaj ostatak 
                //ako nemamo ostatatk samo cemo obrisati poslednji clan liste.
                //Ova while petlja se ne menja mnogo. Popunjavacemo filePartData tako sto cemo pristupati direktno preko indeksa

                int filePartDataListCount = filePartDataCount(fileData->filePartDataList);

                response->filePartData = (filePartDataResponse*)malloc((1 + filePartDataListCount) * sizeof(filePartDataResponse));
                response->partsCount = filePartDataListCount;
                
                while(iterator != NULL)
                {

                    response->responseSize += sizeof(filePartDataResponse);
                    if (iterator->ipClientSocket.sin_addr.S_un.S_addr == inet_addr("0.0.0.0") && ntohs(iterator->ipClientSocket.sin_port) == 0) {
                        
                        //ovo koristimo da pamtimo pocetnu adresu prvog dela fajla na koji naidjemo a da nije smesten ni na jednom klijentu
                        if (saveAddress) {
                            filePartAddress = iterator->filePartAddress;
                            saveAddress = 0;
                        }
                        //nama je sad ovo 0, znaci nemamo da je ovaj deo fajla na klijentu
                        response->filePartData[i].ipClientSocket = iterator->ipClientSocket;
                        response->filePartData[i].filePartSize = htonl(iterator->filePartSize);
                        response->filePartData[i].relativeAddress = htonl(relativeAddress);

                        relativeAddress += iterator->filePartSize;

                        response->filePartData[i].filePartAddress = (char*)malloc(iterator->filePartSize * sizeof(char));
                        memcpy(response->filePartData[i].filePartAddress, iterator->filePartAddress, iterator->filePartSize);
                        response->responseSize += iterator->filePartSize * sizeof(char);
                        summedFilePartSize += iterator->filePartSize;
                       
                        
                    }
                    else {
                        //ovde imamo da je deo fajla kod nekog klijenta, tako da treba da upisemo podatke o klijentu
                        /*
                        KAda saljemo podatke o delu fajla koji je na nekom klijentu, stavljamo da je adresa u response NULL
                        da klijent ne bi mogao samo da cita sa te adrese, nego da mora da se konektuje na drugog klijenta. On ce znati
                        gde da smesta te delove fajla koje je nabavio od drugih klijenata tako sto cemo mu poslati relativnu adresu
                        tog dela fajla, odnosno razliku pocetne adrese celog fajla i pocetne adrese tog dela fajla. Tu relativnu adresu
                        cemo slati za svaki slicaj i kada saljemo cist deo fajla sa servera
                        U strukturu koju saljemo klijentima stavimo jos jedno polje relativne adrese
                        */
                        
                        response->filePartData[i].ipClientSocket = iterator->ipClientSocket;
                        response->filePartData[i].filePartSize = htonl(iterator->filePartSize);
                        response->filePartData[i].relativeAddress = htonl(relativeAddress);

                        relativeAddress += iterator->filePartSize;

                        response->filePartData[i].filePartAddress = (char*)malloc(sizeof(char*));
                        *((int*)response->filePartData[i].filePartAddress) = NULL;
                        filePartOnClientCounter++;
                        response->responseSize += sizeof(char*);
                    }
                    //response->filePartData += sizeof(filePartDataResponse);
                    i++;
                    //response->partsCount = i;
                    iterator = iterator->nextPart;
                }
                //vracamo adresu na pocetak niza jer smo je tokom iteriranja stalno pomerali unapred
                //response->filePartData = firstFilePartData;

                //proveravamo da li posle ovih delova fajla imamo jos ostatak cistog fajla
                if (!(ntohl(response->filePartData[i-1].relativeAddress) + ntohl(response->filePartData[i-1].filePartSize) >= strlen(*(fileData->completeFile)))) {
                   
                    //sabiramo velicinu svih elemenata strukture osim pokazivaca na char a umesto njega dodajemo
                    //i zbir velicina njihovih delova fajlova sto je ustvari relativna adresa poslednjeg elementa liste
                    //relativna adresa predstavlja zbir duzina delova fajlova svih prethodnih elemenata
                    //response->filePartData += i * sizeof(filePartDataResponse);
                    //response->filePartData = (filePartDataResponse*)malloc(1 * sizeof(filePartDataResponse));
                    response->responseSize += sizeof(filePartDataResponse);

                    //stavimo u poslednji element filePartData stavimo pocetnu adresu ostatka fajla a dobijemo je tako sto saberemo adresu pocetka
                    //celog fajla i na nju dodamo relativnu pocetnu adresu poslednjeg dela fajla iz naseg iteriranja i njegovu velicinu
                    char* lastPartAddress = *(fileData->completeFile) + relativeAddress; // ntohl(response->filePartData[i-1].relativeAddress) + ntohl(response->filePartData[i-1].filePartSize);
                    
                    //ovo koristimo da pamtimo pocetnu adresu prvog dela fajla na koji naidjemo a da nije smesten ni na jednom klijentu
                    if (saveAddress) {
                        filePartAddress = lastPartAddress;
                        saveAddress = 0;
                    }

                    int completeFileSize = strlen(*(fileData->completeFile));
                    int sizeOfTheLastPart = completeFileSize - relativeAddress;
                    
                    response->filePartData[i].filePartAddress = (char*)malloc(sizeOfTheLastPart * sizeof(char));
                    response->responseSize += sizeOfTheLastPart * sizeof(char);
                    memcpy(response->filePartData[i].filePartAddress, lastPartAddress, sizeOfTheLastPart);
                    

                    response->partsCount += 1;
                    response->filePartData[i].filePartSize = htonl(sizeOfTheLastPart);
                    response->filePartData[i].relativeAddress = htonl(relativeAddress);
                    response->filePartData[i].ipClientSocket.sin_port = htons(0);
                    response->filePartData[i].ipClientSocket.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
                    summedFilePartSize += (sizeOfTheLastPart + 1);
                }
                /*else {
                    free(&response->filePartData[i]);
                }*/

                //posto smo response->filePartData menjali u iteracijama, onda ce ona pokazivati na poslednji clan u nizu,
                //ovom linijom moramo da vratimo da respon
                //response->filePartData = firstFilePartData;
            }

            response->partsCount = htonl(response->partsCount);
            response->responseSize = htons(response->responseSize);

            fileDataResponseSerialized* serializedResponse = (fileDataResponseSerialized*)malloc(sizeof(fileDataResponseSerialized));
            serializedResponse->responseSize = response->responseSize;
            serializedResponse->partsCount = response->partsCount;

            int sizeOfSerializedFilePartData = ntohl(response->partsCount) * (sizeof(sockaddr_in) + 2 * sizeof(int)) +
                (sizeof(char*) * filePartOnClientCounter) + summedFilePartSize * sizeof(char);

            serializedResponse->filePartData = (char*)malloc(sizeOfSerializedFilePartData * sizeof(char));
            char* savedAddress = NULL;
            
            for (int i = 0; i < ntohl(response->partsCount); i++) {
                if (i == 0)
                {
                    savedAddress = serializedResponse->filePartData;
                }
     
                memcpy(serializedResponse->filePartData, &(response->filePartData[i].ipClientSocket), sizeof(sockaddr_in));
                serializedResponse->filePartData += sizeof(sockaddr_in);

                memcpy(serializedResponse->filePartData, &(response->filePartData[i].filePartSize), sizeof(int));
                serializedResponse->filePartData += sizeof(int);

                memcpy(serializedResponse->filePartData, &(response->filePartData[i].relativeAddress), sizeof(int));
                serializedResponse->filePartData += sizeof(int);

                //filePartAddress ima vrednost NULL kada deo fajla nije ni na jednom klijentu
                if(*(response->filePartData[i].filePartAddress) != NULL)
                {
                    memcpy(serializedResponse->filePartData, response->filePartData[i].filePartAddress, sizeof(char)* ntohl(response->filePartData[i].filePartSize));
                    serializedResponse->filePartData += sizeof(char) * ntohl(response->filePartData[i].filePartSize);
                }
                else {
                    *((int*)serializedResponse->filePartData) = NULL;
                    ((char*)serializedResponse->filePartData) += sizeof(char*); //ovde je velicina NULL 
                }
            }

            serializedResponse->filePartData = savedAddress;
                                                                                                    //ovde sam stavio int umesto short
            short finalResponseSize = sizeof(char) * (sizeOfSerializedFilePartData + sizeof(long) + sizeof(int) + sizeof(short));
            long finalPartsCount = ntohl(serializedResponse->partsCount);
            char* valueResponseSerialized = (char*)malloc(sizeof(char) * finalResponseSize);
            savedAddress = valueResponseSerialized;

            //izmenio sam tako da umesto final response size ovde stavim summedFilePartSize da bih klijentu poslao koliko je bajtova jos smesteno na serveru a ne na drugim klijentima
            int allowedBufferSize = (int)clientRequest->bufferSize;
            if (summedFilePartSize < (int)clientRequest->bufferSize)
            {
                allowedBufferSize = summedFilePartSize - 1;
            }
            memcpy(valueResponseSerialized, &finalResponseSize, sizeof(short));
            valueResponseSerialized += sizeof(short);
            memcpy(valueResponseSerialized, &allowedBufferSize, sizeof(int));
            valueResponseSerialized += sizeof(int);
            memcpy(valueResponseSerialized, &finalPartsCount, sizeof(long));
            valueResponseSerialized += sizeof(long);
            memcpy(valueResponseSerialized, serializedResponse->filePartData, sizeOfSerializedFilePartData);
            valueResponseSerialized = savedAddress;


            //free serialized response
            free(serializedResponse->filePartData);
            free(serializedResponse);

            int sentBytes = 0;
            do {                                                                         //ntohs
                iResult = send(tParameters->acceptedSocket, (char*)valueResponseSerialized + sentBytes, finalResponseSize, 0);
                if (iResult == SOCKET_ERROR)
                {
                    EnterCriticalSection(&(tParameters->printCS));
                    printf("send failed with error: %d\n", WSAGetLastError());
                    LeaveCriticalSection(&(tParameters->printCS));
                    closesocket(tParameters->acceptedSocket);
                    WSACleanup();
                    break;
                }
                sentBytes += iResult;
            } while (finalResponseSize - sentBytes > 0);
           

            //ako je ceo odgovor uspesno poslat radimo upis u hes mapu
            if (sentBytes - finalResponseSize == 0 && allowedBufferSize > 0) {
                //upis u hes mapu
                if (fileData->filePartDataList == NULL) {
                    EnterCriticalSection(&(tParameters->hashMapCS));
                    //fileData.filePartDataList = (filePartData*)malloc(sizeof(filePartData));
                    insertAtEnd(&(fileData->filePartDataList), createNewFilePartData(
                        clientRequest->fileId, tParameters->clientAddr, filePartAddress, allowedBufferSize));
                    LeaveCriticalSection(&(tParameters->hashMapCS));
                }
                else {
                    EnterCriticalSection(&(tParameters->hashMapCS));
                    int updateFailed = updateFilePartData(&(fileData->filePartDataList), tParameters->clientAddr);
                    if (updateFailed) {
                        insertAtEnd(&(fileData->filePartDataList), createNewFilePartData(
                            clientRequest->fileId, tParameters->clientAddr, filePartAddress, allowedBufferSize));
                    }
                    LeaveCriticalSection(&(tParameters->hashMapCS));
                }

                //korisnik je skinuo sad neki novi fajl pa moramo da ga izbrisemo iz evidencije za prethodni fajl
                //AKO BUDEMO IMPLEMENTIRALI DA KLIJENT CUVA VISE DELOVA RAZLICITIH FAJLOVA ONDA CEMO VEROVATNO OVO IZBACITI
                if (lastRequestedFileId != -1) {
                    EnterCriticalSection(&tParameters->hashMapCS);
                    hashValue* fileDataPointer = hmSearch(tParameters->hashMap, (int)lastRequestedFileId);
                    //hashValue fileData = *fileDataPointer;
                    deleteFilePartDataLogical(&(fileDataPointer->filePartDataList), tParameters->clientAddr);
                    LeaveCriticalSection(&tParameters->hashMapCS);
                }     
            }
            
            //OVDE STAVIM DODAVANJE U LISTU ID-OVA
            if (allowedBufferSize > 0)
            {
                lastRequestedFileId = clientRequest->fileId;
            }

            int partCount = ntohs(response->partsCount);
            //ntohs
            for (int i = 0; i < partCount; i++)
            {
                free(response->filePartData[i].filePartAddress);
            }
            free(response->filePartData);
            free(response);

            free(valueResponseSerialized);
            
        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
            EnterCriticalSection(&(tParameters->printCS));
            printf("Connection with client closed.\n");
            LeaveCriticalSection(&(tParameters->printCS));
            closesocket(tParameters->acceptedSocket);
            break;
        }
        else
        {
            // there was an error during recv
            EnterCriticalSection(&(tParameters->printCS));
            printf("recv failed with error: %d\n", WSAGetLastError());
            LeaveCriticalSection(&(tParameters->printCS));
            closesocket(tParameters->acceptedSocket);
            break;
        }
    } while (!*(tParameters->exitFlag));

    /*
           kada se klijent diskonektuje, moramo da update hash mapu tj. da sockaddr stavimo na 0
           prvo pretrazimo hash mapu po file id i tako nadjemo taj node u hash mapi. Dobijemo pokazivac na strukturu
           koja ima pokazivac na ceo fajl i listu filePartData. Prolazimo kroz tu listu i trazimo filePartData koji ima isti
           sockaddr kao i klijent sa kojim komuniciramo na ovom threadu. Tom filePartData stavimo 0 na adresu i port socketa(logicko brisanje)
    */

    /*
        Ako budem implementirao da klijent moze da cuva vise delova razlicitih fajlova, mogao bih na serveru da na svaki novi zahtev dodajem u listu intova i onda ovde da prolazim kroz nju
        i redom brisem za svaki fajl podatke o delu tog fajla na tom klijentu
        AKO LISTA ID-OVA NIJE PRAZNA PROLAZIM KROZ NJU I REDOM BRISEM IZ HES MAPE
    */
    if (lastRequestedFileId != -1) {
        EnterCriticalSection(&tParameters->hashMapCS);
        hashValue* fileDataPointer = hmSearch(tParameters->hashMap, (int)lastRequestedFileId);
        deleteFilePartDataLogical(&(fileDataPointer->filePartDataList), tParameters->clientAddr);
        LeaveCriticalSection(&tParameters->hashMapCS);
    }
    
    //zatvaranje thread-a i close socket 
    EnterCriticalSection(&(tParameters->threadListCS));
    threadNode* nodeToDelete = findthreadNodeByThreadId(*(tParameters->head), tParameters->threadId);
    LeaveCriticalSection(&(tParameters->threadListCS));

    
    //ovde radimo onu logiku za brisanje iz hash mape tj. logicko brisanje
    //treba da se to radi u critical section za menjanje mape. 

    CloseHandle(nodeToDelete->thread);

    EnterCriticalSection(&(tParameters->threadListCS));
    deletethreadNode(tParameters->head, tParameters->threadId);
    //printf("Ispis u client communication thread-u\n");
    //printList(*(tParameters->head));
    LeaveCriticalSection(&(tParameters->threadListCS));
    
    free(tParameters);
    return 0;
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
            EnterCriticalSection(&(ltParams->printCS));
            printf("Connection with client closed.\n");
            LeaveCriticalSection(&(ltParams->printCS));
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
                EnterCriticalSection(&(ltParams->printCS));
                printf("accept failed with error: %d\n", WSAGetLastError());
                LeaveCriticalSection(&(ltParams->printCS));
                closesocket(ltParams->listenSocket);
                WSACleanup();
                break;
            }

            //sending to client his socket data
            int iResult = send(ltParams->acceptedSocket, (char*)&clientAddr, sizeof(sockaddr_in), 0);

            if (iResult == SOCKET_ERROR)
            {
                EnterCriticalSection(&(ltParams->printCS));
                printf("send failed with error: %d\n", WSAGetLastError());
                LeaveCriticalSection(&(ltParams->printCS));
                closesocket(ltParams->acceptedSocket);
                WSACleanup();
                break;
            }

            clientCommunicationThreadParameters* tParameters = (clientCommunicationThreadParameters*)malloc(sizeof(clientCommunicationThreadParameters)); //ovde alocirati memoriju mallocom.
            tParameters->acceptedSocket = ltParams->acceptedSocket;
            tParameters->clientAddr = clientAddr;
            tParameters->printCS = ltParams->printCS;
            tParameters->hashMapCS = ltParams->hashMapCS;
            tParameters->threadListCS = ltParams->threadListCS;
            tParameters->threadId = 0;
            tParameters->exitFlag = ltParams->exitFlag;     //umesto return value zvacemo ga exit koji cemo upisivati u main threa-du
            tParameters->head = ltParams->head;             //dodacemo i polje head pokazivac na listu thread=ova koji kominiciraju sa klijentom.
            tParameters->hashMap = ltParams->hashMap;

            HANDLE clientCommunicaitonThread = CreateThread(NULL, 0, &clientThreadFunction, tParameters, 0, &(tParameters->threadId));

            EnterCriticalSection(&(ltParams->threadListCS));
            printf("Thread ID(inside the listen thread of server function, id of the thread for communication with client): %d\n", tParameters->threadId);
            threadNode* tn = createNewThreadNode(clientCommunicaitonThread, tParameters->threadId);
            insertAtHead(ltParams->head, tn);
            printList(*(ltParams->head));
            LeaveCriticalSection(&(ltParams->threadListCS));
        }
    } while (!*(ltParams->exitFlag));

    EnterCriticalSection(&(ltParams->threadListCS));
    threadNode* nodeToClose = findthreadNodeByThreadId(*(ltParams->head), ltParams->threadId);
    LeaveCriticalSection(&(ltParams->threadListCS));

    CloseHandle(nodeToClose->thread);
    
    //brisanje thread node iz liste
    EnterCriticalSection(&(ltParams->threadListCS));
    deletethreadNode(ltParams->head, ltParams->threadId);
    LeaveCriticalSection(&(ltParams->threadListCS));

    return 0;
}

