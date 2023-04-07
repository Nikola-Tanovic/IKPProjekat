#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include "structures.h";
#include "threadList.h";
#include "filePartDataList.h";

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27016

// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.
bool InitializeWindowsSockets();


typedef struct listenThreadParameters {
    unsigned short clientPort;
    CRITICAL_SECTION bufferCS;
    CRITICAL_SECTION printCS;
    char** clientBuff;
    int* exitFlag;
}listenThreadParameters;

typedef struct serverThreadParameters {
    SOCKET serverConnectSocket;
    char** clientBuffer;
    CRITICAL_SECTION bufferCS;
    CRITICAL_SECTION printCS;
    int* exitFlag;
}serverThreadParameters;

typedef struct getFilePartDataThreadParameters {
    CRITICAL_SECTION printCS;
    CRITICAL_SECTION threadListCS;
    CRITICAL_SECTION filePartDataListCS;
    filePartDataResponse filePartData;
    DWORD threadId;
    threadNode** head;
    filePartDataNode** filePartDataListHead;
}getFilePartDataTHreadParameters;

DWORD WINAPI serverThreadFunction(LPVOID lpParam);
DWORD WINAPI listenThreadFunction(LPVOID lpParam);
DWORD WINAPI getFilePartDataThreadFunction(LPVOID lpParam);

int __cdecl main(int argc, char** argv)
{
    // socket used to communicate with server
    SOCKET serverConnectSocket = INVALID_SOCKET;
    // variable used to store function return value
    int iResult;

    char* recvBuff = (char*)malloc(DEFAULT_BUFLEN);
    
    int exitFlag = 0;

    int result = 0;

    //ovde ce se smestati deo fajla koji se cuva kod klijenta
    char* clientBuffer = (char*)malloc(sizeof(char*));


    CRITICAL_SECTION printCS;
    InitializeCriticalSection(&printCS);

    CRITICAL_SECTION bufferCS;
    InitializeCriticalSection(&bufferCS);

    if (InitializeWindowsSockets() == false)
    {
        // we won't log anything since it will be logged
        // by InitializeWindowsSockets() function
        return 1;
    }

    // create a socket
    serverConnectSocket = socket(AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP);

    if (serverConnectSocket == INVALID_SOCKET)
    {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // create and initialize address structure
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(DEFAULT_PORT);
    // connect to server specified in serverAddress and socket serverConnectSocket
    if (connect(serverConnectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
    {
        printf("Unable to connect to server.\n");
        closesocket(serverConnectSocket);
        WSACleanup();
    }
    //moramo da dobavimo od servera podatke o klijent soketu da bi znali u listenThreadu na sta da bindujemo listen socket
    sockaddr_in clientAddr;
    
    iResult = recv(serverConnectSocket, recvBuff, DEFAULT_BUFLEN, 0);
    if (iResult > 0)
    {
        recvBuff[iResult] = '\0';
        clientAddr = *((sockaddr_in*)recvBuff);
        printf("Client socket is: PORT %d, IP ADDRESS %s\n", ntohs(clientAddr.sin_port), inet_ntoa(clientAddr.sin_addr));
    }
    else if (iResult == 0)
    {
        // connection was closed gracefully
        printf("Connection with client closed.\n");
        closesocket(serverConnectSocket);
    }
    else
    {
        // there was an error during recv
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(serverConnectSocket);
    }
    //ovde pozivamo listenThread koji slusa zahteve od drugih klijenata
    //clientSocket, bufferCS, printCS, clientBuff, 

    free(recvBuff);

    unsigned short clientPort = ntohs(clientAddr.sin_port);

    listenThreadParameters* ltParams = (listenThreadParameters*)malloc(sizeof(listenThreadParameters));
    ltParams->bufferCS = bufferCS;
    ltParams->printCS = printCS;
    ltParams->clientPort = clientPort + 1;
    ltParams->clientBuff = &clientBuffer;
    ltParams->exitFlag = &exitFlag;

    DWORD listenThreadID;
    HANDLE listenThread = CreateThread(NULL, 0, &listenThreadFunction, ltParams, 0, &listenThreadID);

    serverThreadParameters* stParams = (serverThreadParameters*)malloc(sizeof(serverThreadParameters));
    stParams->bufferCS = bufferCS;
    stParams->printCS = printCS;
    stParams->clientBuffer = &clientBuffer;
    stParams->serverConnectSocket = serverConnectSocket;
    stParams->exitFlag = &exitFlag;

    DWORD serverThreadID;
    HANDLE serverThread = CreateThread(NULL, 0, &serverThreadFunction, stParams, 0, &serverThreadID);

    //OVDE ZA SADA IMAMO NIZ THREADOVA FIKSNE DUZINE, TO CE SE PROMENITI U LISTU THREADOVA KAO NA SERVERU STO IMAMO 
    HANDLE handles[2];
    handles[0] = listenThread;
    handles[1] = serverThread;

    while (1) {
        if (exitFlag == -2) {
            while (WaitForMultipleObjects(2, handles, TRUE, INFINITE)) {

            }
            DeleteCriticalSection(&printCS);
            DeleteCriticalSection(&bufferCS);
            free(clientBuffer);
            //free(&clientBuffer);
            free(ltParams);
            free(stParams);
            CloseHandle(handles[0]);
            CloseHandle(handles[1]);
            
            return 0;
        }
        else {
            Sleep(200);
        }
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

DWORD WINAPI listenThreadFunction(LPVOID lpParam)
{
    listenThreadParameters* ltParams = (listenThreadParameters*)lpParam;

    // Socket used for listening for new clients 
    SOCKET listenSocket = INVALID_SOCKET;
    // Socket used for communication with client
    SOCKET acceptedSocket = INVALID_SOCKET;
    // variable used to store function return value
    int iResult;
    // Buffer used for storing incoming data
    char recvbuf[DEFAULT_BUFLEN];

    unsigned long mode = 1;
    int result = 0;



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

    //zbog toga sto getaddr info mora da primi char* kao parametar funkcije
    //potrebno je da port iz unsigned short formata prebacimo u char* format
    char port[6];
    port[5] = '\0';
    itoa(ltParams->clientPort, port, 10);


    // Resolve the server address and port
    iResult = getaddrinfo(NULL, port, &hints, &resultingAddress);
    if (iResult != 0)
    {
        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)getaddrinfo failed with error: %d\n", iResult);
        LeaveCriticalSection(&ltParams->printCS);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    listenSocket = socket(AF_INET,      // IPv4 address famly
        SOCK_STREAM,  // stream socket
        IPPROTO_TCP); // TCP

    if (listenSocket == INVALID_SOCKET)
    {
        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)socket failed with error: %ld\n", WSAGetLastError());
        LeaveCriticalSection(&ltParams->printCS);
        freeaddrinfo(resultingAddress);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket - bind port number and local address 
    // to socket
    iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)bind failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&ltParams->printCS);
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
        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)listen failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&ltParams->printCS);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    EnterCriticalSection(&ltParams->printCS);
    printf("\n(listenThread)Listen client socket initialized, waiting for other clients.\n");
    printf("(listenThread)Listen socket port: %hu\n", ltParams->clientPort);
    LeaveCriticalSection(&ltParams->printCS);

    iResult = ioctlsocket(listenSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR) {
        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)ioctlsocket failed with error: %ld\n", iResult);
        printf("(listenThread) failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&ltParams->printCS);
    }
    fd_set readfds;
    do
    {
        FD_ZERO(&readfds);

        FD_SET(listenSocket, &readfds);

        timeval timeVal;
        timeVal.tv_sec = 1;
        timeVal.tv_usec = 0;

        result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            EnterCriticalSection(&ltParams->printCS);
            printf("(listenThread)Connection with client closed.\n");
            LeaveCriticalSection(&ltParams->printCS);
            closesocket(acceptedSocket);
            break;
        }

        // Wait for clients and accept client connections.
        // Returning value is acceptedSocket used for further
        // Client<->Server communication. This version of
        // server will handle only one client.
        acceptedSocket = accept(listenSocket, NULL, NULL);

        if (acceptedSocket == INVALID_SOCKET)
        {
            EnterCriticalSection(&ltParams->printCS);
            printf("(listenThread)accept failed with error: %d\n", WSAGetLastError());
            LeaveCriticalSection(&ltParams->printCS);
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        EnterCriticalSection(&ltParams->bufferCS);
        char* messageToSend = *ltParams->clientBuff;
        LeaveCriticalSection(&ltParams->bufferCS);

        iResult = send(acceptedSocket, messageToSend, (int)strlen(messageToSend) + 1, 0);
        if (iResult == SOCKET_ERROR)
        {
            EnterCriticalSection(&ltParams->printCS);
            printf("(listenThread)send failed with error: %d\n", WSAGetLastError());
            LeaveCriticalSection(&ltParams->printCS);
            closesocket(acceptedSocket);
            WSACleanup();
            return 1;
        }

        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)Bytes sent to other client: %ld\n", iResult);
        LeaveCriticalSection(&ltParams->printCS);

        FD_CLR(acceptedSocket, &readfds);

    } while (*ltParams->exitFlag != -2);

    // shutdown the connection since we're done
    iResult = shutdown(acceptedSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        EnterCriticalSection(&ltParams->printCS);
        printf("(listenThread)shutdown failed because no other client has connected yet.");
        LeaveCriticalSection(&ltParams->printCS);
        closesocket(acceptedSocket);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(listenSocket);
    closesocket(acceptedSocket);
    WSACleanup();

    return 0;
}

DWORD WINAPI serverThreadFunction(LPVOID lpParam) {

    serverThreadParameters* stParams = (serverThreadParameters*)lpParam;

    long initialServerResponse = -1;

    int result = 0;

    
    // message to send
    request requestMessage;

    //serverConnectSocket, clientBuff, bufferCS, printCS
    //odavde ide thread za komunikaciju sa serverom
    //prebacujemo u neblokirajuci rezim
    unsigned long mode = 1; //non-blocking mode
    int iResult = ioctlsocket(stParams->serverConnectSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);


    fd_set writefds;
    fd_set readfds;

    timeval timeVal;
    timeVal.tv_sec = 0;
    timeVal.tv_usec = 0;

    //inicijalno dobijanje koliko fajlova ima na serveru
    do {
        FD_ZERO(&readfds);
        FD_SET(stParams->serverConnectSocket, &readfds);
        int result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
            //Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            EnterCriticalSection(&stParams->printCS);
            printf("(serverThread)Connection with server closed.\n");
            LeaveCriticalSection(&stParams->printCS);
            closesocket(stParams->serverConnectSocket);
            break;
        }

        iResult = recv(stParams->serverConnectSocket, (char*)&initialServerResponse, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            initialServerResponse = ntohl((long)initialServerResponse);
            EnterCriticalSection(&stParams->printCS);
            printf("(serverThread)Number of avaliable files on server: %ld\n", initialServerResponse);
            LeaveCriticalSection(&stParams->printCS);
            break;
        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
            EnterCriticalSection(&stParams->printCS);
            printf("(serverThread)Connection with client closed.\n");
            LeaveCriticalSection(&stParams->printCS);
            closesocket(stParams->serverConnectSocket);
            break;
        }
        else
        {
            EnterCriticalSection(&stParams->printCS);
            // there was an error during recv
            printf("(serverThread)recv failed with error: %d\n", WSAGetLastError());
            LeaveCriticalSection(&stParams->printCS);
            closesocket(stParams->serverConnectSocket);
            break;
        }
    } while (initialServerResponse == -1);

    //prevent the user to request the same file two times in a row
    long lastRequestedFileId = -3;

    while (1) {
        FD_ZERO(&writefds);

        FD_SET(stParams->serverConnectSocket, &writefds);

        result = select(0, NULL, &writefds, NULL, &timeVal);

        if (result == 0) {
            EnterCriticalSection(&stParams->printCS);
            printf("(serverThread)Time for waiting has passed. (select before send function - Client)\n");
            LeaveCriticalSection(&stParams->printCS);
            Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            EnterCriticalSection(&stParams->printCS);
            // connection was closed gracefully
            printf("(serverThread)Connection with client closed.\n");
            LeaveCriticalSection(&stParams->printCS);
            closesocket(stParams->serverConnectSocket);
            break;
        }

        long fileId = -1;

        do {
            EnterCriticalSection(&stParams->printCS);
            printf("Enter file id: ");
            LeaveCriticalSection(&stParams->printCS);
            scanf("%ld", &fileId);
           
            requestMessage.fileId = htonl(fileId);
        } while ((fileId >= initialServerResponse || fileId < 0 || fileId == lastRequestedFileId) && fileId != -2);

        lastRequestedFileId = fileId;

        //ovde se menja vrednost exitFlaga-a i eventualno izlazi iz funkcije 
        // sa obzirom da se samo ovde moze zahtevati zatvaranje ovog klijenta, vrv necemo morati da proveravamo exitFlag u novom thread koji cemo implementirati za dobavljanje dela fajla 
        //nego ce se taj thread svakako gasiti cim dobavi deo fajla jer nema potrebe da dalje radi 
        if (fileId == -2) {
            *stParams->exitFlag = -2;
            break;
        }

        char* recvBuff = (char*)malloc(sizeof(char) * DEFAULT_BUFLEN);
        char* startRecvBuff = recvBuff;


        long bufferSize = -1;
        EnterCriticalSection(&stParams->printCS);
        printf("\nEnter the size of the file you want to store in your client buffer: ");
        LeaveCriticalSection(&stParams->printCS);
        scanf("%ld", &bufferSize);
        requestMessage.bufferSize = htonl(bufferSize);

        //ovo zauzimanje memorije za clientBuff bi mozda mogli da stavimo u 685 liniju u sustini nakon sto dobijemo odgovor od servera u koji cemo implemetirati da pored svega ostalog salje i
        //broj bajtova koji nisu ni na jendom klijentu da bi ako tipa mi trazimo 5 a on u stvari ima samo 2, mi stavili bufferSize na 2 a ne 5, a ako je 0:
        //1)onda samo prosto nista ne stavimo u bafer
        /*
        EnterCriticalSection(&stParams->bufferCS);
        *(stParams->clientBuffer) = (char*)malloc(sizeof(char) * bufferSize);
        LeaveCriticalSection(&stParams->bufferCS);
        */

        // send koji salje zahtev serveru gde se navodi idFile i velicina dela fajla koji klijent cuva kod sebe
        iResult = send(stParams->serverConnectSocket, (char*)&requestMessage, (int)sizeof(request), 0);

        if (iResult == SOCKET_ERROR)
        {
            EnterCriticalSection(&stParams->printCS);
            printf("(serverThread)Send failed with error: %d\n", WSAGetLastError());
            LeaveCriticalSection(&stParams->printCS);
            closesocket(stParams->serverConnectSocket);
            WSACleanup();
            return 1;
        }

        EnterCriticalSection(&stParams->printCS);
        printf("\n(serverThread)Request sent: %ld bytes\n", iResult);
        LeaveCriticalSection(&stParams->printCS);

        FD_CLR(stParams->serverConnectSocket, &writefds);

        fileDataResponse* serverResponse = (fileDataResponse*)malloc(sizeof(fileDataResponse));

        int recievedBytes = 0;
        int responseSizeRecieved = 0;
        short responseSize = 1;
        do {
            FD_ZERO(&readfds);
            FD_SET(stParams->serverConnectSocket, &readfds);
            int result = select(0, &readfds, NULL, NULL, &timeVal);

            if (result == 0) {
                /*
                EnterCriticalSection(&stParams->printCS);
                printf("(serverThread)Time for waiting has passed. (select before recv function - Client)\n");
                LeaveCriticalSection(&stParams->printCS);
                */
                Sleep(1000);
                continue;
            }
            else if (result == SOCKET_ERROR) {
                EnterCriticalSection(&stParams->printCS);
                printf("(serverThread)Connection with server closed.\n");
                LeaveCriticalSection(&stParams->printCS);
                closesocket(stParams->serverConnectSocket);
                break;
            }

            //recieve of file part data from server
            iResult = recv(stParams->serverConnectSocket, recvBuff + recievedBytes, DEFAULT_BUFLEN, 0);
            if (iResult > 0) {
                if (responseSizeRecieved == 0) {
                    responseSize = *((short*)recvBuff);
                    recvBuff += sizeof(short); 
                    responseSizeRecieved = 1;
                }

                FD_CLR(stParams->serverConnectSocket, &readfds);
                recievedBytes += iResult;
            }
            else if (iResult == 0)
            {
                // connection was closed gracefully
                EnterCriticalSection(&stParams->printCS);
                printf("(serverThread)Connection with client closed.\n");
                LeaveCriticalSection(&stParams->printCS);
                closesocket(stParams->serverConnectSocket);
                break;
            }
            else
            {
                // there was an error during recv
                EnterCriticalSection(&stParams->printCS);
                printf("(serverThread)recv failed with error: %d\n", WSAGetLastError());
                LeaveCriticalSection(&stParams->printCS);
                closesocket(stParams->serverConnectSocket);
                break;
            }


        } while ((int)responseSize - recievedBytes > 0);


        if (responseSize == recievedBytes) {

            serverResponse->responseSize = responseSize;
            serverResponse->allowedBufferSize = *((int*)recvBuff);
            recvBuff += sizeof(int);
            serverResponse->partsCount = *((long*)recvBuff);
            recvBuff += sizeof(long);

            EnterCriticalSection(&stParams->printCS);
            printf("\nAllowed buffer size: %d", serverResponse->allowedBufferSize);
            printf("\nParts count: %d\n", serverResponse->partsCount);
            LeaveCriticalSection(&stParams->printCS);
            filePartDataResponse* savedAddress = NULL;

            serverResponse->filePartData = (filePartDataResponse*)malloc(sizeof(filePartDataResponse) * serverResponse->partsCount);

            for (int i = 0; i < serverResponse->partsCount; i++) {

                if (i == 0)
                {
                    savedAddress = serverResponse->filePartData;
                }

                memcpy(&serverResponse->filePartData[i].ipClientSocket, recvBuff, sizeof(sockaddr_in));
                recvBuff += sizeof(sockaddr_in);

                memcpy(&serverResponse->filePartData[i].filePartSize, recvBuff, sizeof(int));
                serverResponse->filePartData[i].filePartSize = ntohl(serverResponse->filePartData[i].filePartSize);
                recvBuff += sizeof(int);

                memcpy(&serverResponse->filePartData[i].relativeAddress, recvBuff, sizeof(int));
                serverResponse->filePartData[i].relativeAddress = ntohl(serverResponse->filePartData[i].relativeAddress);
                recvBuff += sizeof(int);

                serverResponse->filePartData[i].ipClientSocket.sin_port = ntohs(serverResponse->filePartData[i].ipClientSocket.sin_port);

                if (serverResponse->filePartData[i].ipClientSocket.sin_port == 0)
                {
                    serverResponse->filePartData[i].filePartAddress = (char*)malloc(serverResponse->filePartData[i].filePartSize * sizeof(char));
                    memcpy(serverResponse->filePartData[i].filePartAddress, recvBuff, serverResponse->filePartData[i].filePartSize);

                    serverResponse->filePartData[i].filePartAddress[serverResponse->filePartData[i].filePartSize] = '\0';
                    recvBuff += serverResponse->filePartData[i].filePartSize;
                }
                else
                {
                    serverResponse->filePartData[i].filePartAddress = (char*)malloc(sizeof(char*));
                    memcpy(serverResponse->filePartData[i].filePartAddress, recvBuff, sizeof(char*));

                    recvBuff += sizeof(char*);
                }
                EnterCriticalSection(&stParams->printCS);
                printf("\nFile Part size [%d]: %ld", i, serverResponse->filePartData[i].filePartSize);
                printf("\nRelative address[%d]: %ld", i, serverResponse->filePartData[i].relativeAddress);
                printf("\nIP Address of client that we need to connect[%d]: %s", i, inet_ntoa(serverResponse->filePartData[i].ipClientSocket.sin_addr));
                printf("\nPort of client that we need to connect[%d]: %d", i, serverResponse->filePartData[i].ipClientSocket.sin_port);
                if (serverResponse->partsCount > 1 && serverResponse->filePartData[i].ipClientSocket.sin_port == 0)
                {
                    printf("\nPart of file [%d]: %s", i, serverResponse->filePartData[i].filePartAddress);
                }
                printf("\n");
                LeaveCriticalSection(&stParams->printCS);

            }
            
            serverResponse->filePartData = savedAddress;

        }
        //zauzimanje memorijskog prostora za klijent buffer i to velicinu koju je server dozvolio
        int noBufferFlag = 0;

        if (serverResponse->allowedBufferSize > 0)
        {
            EnterCriticalSection(&stParams->bufferCS);
            *(stParams->clientBuffer) = (char*)malloc(sizeof(char) * serverResponse->allowedBufferSize);
            LeaveCriticalSection(&stParams->bufferCS);

            bufferSize = serverResponse->allowedBufferSize;
        }
        else
        {
            noBufferFlag = 1;
        }

        //u print buffer pisemo celokupnu poruku
        char* printBuffer = NULL;
        int clientBufferFull = 0;
        int numberOfWrittenBytes = 0;
        char* startPrintBuffer = NULL;
        int startPrintBufferFlag = 0;
        char* endOfPrintBuffer = NULL;

        //
        //LISTA THREADOVA
        //
        //initializing list of threads
        threadNode* head = NULL;

        //critical section that controls access to threadList
        CRITICAL_SECTION threadListCS;
        InitializeCriticalSection(&threadListCS);

        //critical section that controls access to printBuffer
        CRITICAL_SECTION printBufferCS;
        InitializeCriticalSection(&printBufferCS);

        //initializing list of filePartData which will be sorted eventually and will be used to put the whole downloaded file together
        filePartDataNode* filePartDataListHead = NULL;//(filePartDataNode*)malloc(sizeof(filePartDataNode) * serverResponse->partsCount);

        //critical section that controls access to FilePartDataList
        CRITICAL_SECTION filePartDataListCS;
        InitializeCriticalSection(&filePartDataListCS);

        //free the first recvBuff
        recvBuff = startRecvBuff;
        free(recvBuff);

        //allocate the memory for the whole file 
        int completeFileSize = 0;
        for (int i = 0; i < serverResponse->partsCount; i++) {
            completeFileSize += serverResponse->filePartData[i].filePartSize;
        }

        printBuffer = (char*)malloc(sizeof(char) * (completeFileSize + 1));

        //going through the array of FilePartData
        for (int i = 0; i < serverResponse->partsCount; i++) {
            //SLUCAJ KADA SE RADI O CISTOM DELU FAJLA KOJI NIJE NI NA JEDNOM KLIJENTU
            if (serverResponse->filePartData[i].ipClientSocket.sin_port == 0 &&
                serverResponse->filePartData[i].ipClientSocket.sin_addr.S_un.S_addr == inet_addr("0.0.0.0")) {

                //in this section part of the requested file is stored in client buffer
                if (!clientBufferFull && !noBufferFlag) {
                    //check if the part of the file is bigger or equal in comparison with the client buffer
                    if (serverResponse->filePartData[i].filePartSize - (bufferSize - numberOfWrittenBytes) >= 0) {
                        EnterCriticalSection(&stParams->bufferCS);
                        memcpy(*stParams->clientBuffer + numberOfWrittenBytes, serverResponse->filePartData[i].filePartAddress, bufferSize - numberOfWrittenBytes);
                        LeaveCriticalSection(&stParams->bufferCS);
                        numberOfWrittenBytes += (bufferSize - numberOfWrittenBytes);
                        clientBufferFull = 1;
                    }
                    else {
                        EnterCriticalSection(&stParams->bufferCS);
                        memcpy(*stParams->clientBuffer + numberOfWrittenBytes, serverResponse->filePartData[i].filePartAddress, serverResponse->filePartData[i].filePartSize);
                        LeaveCriticalSection(&stParams->bufferCS);
                        numberOfWrittenBytes += serverResponse->filePartData[i].filePartSize;
                        if (numberOfWrittenBytes == bufferSize) {
                            clientBufferFull = 1;
                        }
                    }
                }

                filePartDataNode* newNode = createNewFilePartDataNode(serverResponse->filePartData[i].relativeAddress, serverResponse->filePartData[i].filePartSize);
                memcpy(newNode->filePartAddress, serverResponse->filePartData[i].filePartAddress, serverResponse->filePartData[i].filePartSize);

                EnterCriticalSection(&filePartDataListCS);
                insertAtHead(&filePartDataListHead, newNode);
                LeaveCriticalSection(&filePartDataListCS);
            }
            else {

                //parts of the requested file are downloaded from other clients
                getFilePartDataThreadParameters* tParameters = (getFilePartDataThreadParameters*)malloc(sizeof(getFilePartDataTHreadParameters));
                tParameters->printCS = stParams->printCS;
                tParameters->filePartData = serverResponse->filePartData[i];
                tParameters->threadId = 0;
                tParameters->head = &head;
                tParameters->threadListCS = threadListCS;
                tParameters->filePartDataListHead = &filePartDataListHead;
                tParameters->filePartDataListCS = filePartDataListCS;
                

                HANDLE getFilePartDataThread = CreateThread(NULL, 0, &getFilePartDataThreadFunction, tParameters, 0, &(tParameters->threadId));
                
                EnterCriticalSection(&threadListCS);
                printf("Thread ID getFilePartData thread-a: %d\n", tParameters->threadId);
                threadNode* tn = createNewThreadNode(getFilePartDataThread, tParameters->threadId);
                insertAtHead(&head, tn);
                //printList(head);
                LeaveCriticalSection(&threadListCS);         
            } 
        }

        //waiting for all threads to finish downloading and to cloes themselves
        while (head != NULL) {
            Sleep(200);
        }
        if (serverResponse->partsCount > 1)
        {
            bubbleSort(filePartDataListHead);
        }

        filePartDataNode* iterator = filePartDataListHead;

        while (iterator != NULL)
        {
            if (!startPrintBufferFlag)
            {
                startPrintBuffer = printBuffer;
                startPrintBufferFlag = 1;
            }
            memcpy(printBuffer, iterator->filePartAddress, iterator->filePartSize);
            printBuffer += iterator->filePartSize;
            endOfPrintBuffer = printBuffer;

            iterator = iterator->next;
        }

        EnterCriticalSection(&stParams->printCS);
        *endOfPrintBuffer = '\0';
        printf("\nWhole file: %s\n", startPrintBuffer);
        LeaveCriticalSection(&stParams->printCS);
       
        //memory deallocation
        //free(printBuffer);
        free(serverResponse);
        //free(recvBuff);
    }
    // cleanup
    closesocket(stParams->serverConnectSocket);
    WSACleanup();


    return 0;
}


DWORD WINAPI getFilePartDataThreadFunction(LPVOID lpParam)
{
    getFilePartDataThreadParameters* tParameters = (getFilePartDataThreadParameters*)lpParam;

    // create a socket
    SOCKET clientConnectSocket = socket(AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP);

    int result = 0;

    if (clientConnectSocket == INVALID_SOCKET)
    {
        EnterCriticalSection(&tParameters->printCS);
        printf("(getFilePartDataThread)socket failed with error: %ld\n", WSAGetLastError());
        LeaveCriticalSection(&tParameters->printCS);
        WSACleanup();
        return 1;
    }

    // create and initialize address structure
    sockaddr_in clientAddress;
    clientAddress.sin_family = AF_INET;

    clientAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientAddress.sin_port = htons(tParameters->filePartData.ipClientSocket.sin_port + 1);

    // connect to server specified in serverAddress and socket serverConnectSocket
    if (connect(clientConnectSocket, (SOCKADDR*)&clientAddress, sizeof(clientAddress)) == SOCKET_ERROR)
    {
        EnterCriticalSection(&tParameters->printCS);
        printf("\nError: %d", WSAGetLastError());
        printf("(getFilePartDataThread)Unable to connect to server.\n");
        LeaveCriticalSection(&tParameters->printCS);
        closesocket(clientConnectSocket);
        WSACleanup();
    }

    unsigned long mode = 1; //non-blocking mode
    int iResult = ioctlsocket(clientConnectSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
        printf("(getFilePartDataThread)ioctlsocket failed with error: %ld\n", iResult);

    fd_set readfds;

    timeval timeVal;
    timeVal.tv_sec = 1;
    timeVal.tv_usec = 0;

    char* recvBuff = (char*)malloc(sizeof(char) * tParameters->filePartData.filePartSize + 1);


    do
    {
        FD_ZERO(&readfds);
        FD_SET(clientConnectSocket, &readfds);

        result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
            Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            EnterCriticalSection(&tParameters->printCS);
            // connection was closed gracefully
            printf("(getFilePartDataThread)Connection with client closed.\n");
            LeaveCriticalSection(&tParameters->printCS);

            closesocket(clientConnectSocket);
            break;
        }

        // Receive data until the client shuts down the connection
        iResult = recv(clientConnectSocket, recvBuff, tParameters->filePartData.filePartSize, 0);

        if (iResult > 0)
        {
            recvBuff[tParameters->filePartData.filePartSize] = '\0';
            EnterCriticalSection(&tParameters->printCS);
            printf("(getFilePartDataThread)Message received from client: %s\n", recvBuff);
            LeaveCriticalSection(&tParameters->printCS);

            filePartDataNode* newNode = createNewFilePartDataNode(tParameters->filePartData.relativeAddress, tParameters->filePartData.filePartSize);
            memcpy(newNode->filePartAddress, recvBuff, tParameters->filePartData.filePartSize);

            EnterCriticalSection(&tParameters->filePartDataListCS);
            insertAtHead(tParameters->filePartDataListHead, newNode);
            LeaveCriticalSection(&tParameters->filePartDataListCS);
            
            free(recvBuff);
            FD_CLR(clientConnectSocket, &readfds);
            break;
        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            printf("(getFilePartDataThread)Connection with client closed.\n");
            closesocket(clientConnectSocket);
            break;
        }
        else
        {
            // there was an error during recv
            printf("(getFilePartDataThread)recv failed with error: %d\n", WSAGetLastError());
            closesocket(clientConnectSocket);
            break;
        }
    } while (1);

    //closing the thread and deleting its threadNode from the list of threadNodes
    EnterCriticalSection(&(tParameters->threadListCS));
    threadNode* nodeToDelete = findthreadNodeByThreadId(*(tParameters->head), tParameters->threadId);
    LeaveCriticalSection(&(tParameters->threadListCS));

    CloseHandle(nodeToDelete->thread);

    EnterCriticalSection(&(tParameters->threadListCS));
    deletethreadNode(tParameters->head, tParameters->threadId);
    LeaveCriticalSection(&(tParameters->threadListCS));

    free(tParameters);
    
    // cleanup
    closesocket(clientConnectSocket);

    return 0;
}
