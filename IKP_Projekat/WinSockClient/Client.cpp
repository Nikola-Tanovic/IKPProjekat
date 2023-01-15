#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include "structures.h";

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27016

typedef struct listenThreadParameters {
    //clientAddr, bufferCS, printCS, clientBuff, 
    unsigned short clientPort;
    CRITICAL_SECTION bufferCS;
    CRITICAL_SECTION printCS;
    char** clientBuff;
    int* exitFlag;
}listenThreadParameters;

typedef struct serverThreadParameters {
    //serverConnectSocket, clientBuff, bufferCS, printCS
    SOCKET serverConnectSocket;
    char** clientBuffer;
    CRITICAL_SECTION bufferCS;
    CRITICAL_SECTION printCS;
    int* exitFlag;
}serverThreadParameters;


// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.
bool InitializeWindowsSockets();

DWORD WINAPI serverThreadFunction(LPVOID lpParam);
DWORD WINAPI listenThreadFunction(LPVOID lpParam);

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
    printf("(listenThread)Listen client socket initialized, waiting for other clients.\n");
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
            printf("(listenThread)send failed with error: %d\n", WSAGetLastError());
            closesocket(acceptedSocket);
            WSACleanup();
            return 1;
        }

        printf("(listenThread)Bytes Sent: %ld\n", iResult);

        FD_CLR(acceptedSocket, &readfds);

    } while (*ltParams->exitFlag != -2);

    // shutdown the connection since we're done
    iResult = shutdown(acceptedSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        printf("(listenThread)shutdown failed because no other client has connected yet.");
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
            printf("(serverThread)Connection with server closed.\n");
            closesocket(stParams->serverConnectSocket);
            break;
        }

        iResult = recv(stParams->serverConnectSocket, (char*)&initialServerResponse, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            initialServerResponse = ntohl((long)initialServerResponse);
            printf("(serverThread)Broj fajlova raspolozivih na serveru je: %ld\n", initialServerResponse);
            break;
        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
            printf("(serverThread)Connection with client closed.\n");
            closesocket(stParams->serverConnectSocket);
            break;
        }
        else
        {
            // there was an error during recv
            printf("(serverThread)recv failed with error: %d\n", WSAGetLastError());
            closesocket(stParams->serverConnectSocket);
            break;
        }
    } while (initialServerResponse == -1);

    while (1) {
        FD_ZERO(&writefds);

        FD_SET(stParams->serverConnectSocket, &writefds);

        result = select(0, NULL, &writefds, NULL, &timeVal);

        if (result == 0) {
            printf("(serverThread)Vreme za cekanje je isteklo (select pre send funkcije - Klijent )\n");
            Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            printf("(serverThread)Connection with client closed.\n");
            closesocket(stParams->serverConnectSocket);
            break;
        }

        long fileId = -1;
        do {
            printf("Unesite id fajla koji zelite: ");
            scanf("%ld", &fileId);
           
            requestMessage.fileId = htonl(fileId);
        } while ((fileId >= initialServerResponse || fileId < 0) && fileId != -2 );

        if (fileId == -2) {
            *stParams->exitFlag = -2;
            break;
        }

        char* recvBuff = (char*)malloc(sizeof(char) * DEFAULT_BUFLEN);
        char* startRecvBuff = recvBuff;


        long bufferSize = -1;
        printf("\nUnesite velicinu fajla koju zelite da smestite kod sebe: ");
        scanf("%ld", &bufferSize);
        requestMessage.bufferSize = htonl(bufferSize);

        EnterCriticalSection(&stParams->bufferCS);
        *(stParams->clientBuffer) = (char*)malloc(sizeof(char) * bufferSize);
        LeaveCriticalSection(&stParams->bufferCS);


        // send koji salje zahtev serveru gde se navodi idFile i velicina dela fajla koji klijent cuva kod sebe
        iResult = send(stParams->serverConnectSocket, (char*)&requestMessage, (int)sizeof(request), 0);

        if (iResult == SOCKET_ERROR)
        {
            printf("(serverThread)send failed with error: %d\n", WSAGetLastError());
            closesocket(stParams->serverConnectSocket);
            WSACleanup();
            return 1;
        }

        printf("(serverThread)Bytes Sent: %ld\n", iResult);

        FD_CLR(stParams->serverConnectSocket, &writefds);

        fileDataResponse* serverResponse = (fileDataResponse*)malloc(sizeof(fileDataResponse));
        //fileDataResponseSerialized* serverResponseSerialized = (fileDataResponseSerialized*)malloc(sizeof(fileDataResponseSerialized));
        //short recievedBytes = 0;

        int recievedBytes = 0;
        int responseSizeRecieved = 0;
        //short* responseSizeP ;
        short responseSize = 1;
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
                printf("(serverThread)Connection with server closed.\n");
                closesocket(stParams->serverConnectSocket);
                break;
            }

            //moze da se desi greska ako saljemo mnogo bajtova da bude prepunjen ovaj default buflen
            iResult = recv(stParams->serverConnectSocket, recvBuff + recievedBytes, DEFAULT_BUFLEN, 0);
            if (iResult > 0) {
                if (responseSizeRecieved == 0) {
                    //recvBuff[iResult] = '\0';
                    responseSize = *((short*)recvBuff);
                    //responseSize = ntohs(responseSize);
                    recvBuff += sizeof(short);        
                }

                FD_CLR(stParams->serverConnectSocket, &readfds);
                recievedBytes += iResult;
            }
            else if (iResult == 0)
            {
                // connection was closed gracefully
                //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
                printf("(serverThread)Connection with client closed.\n");
                closesocket(stParams->serverConnectSocket);
                break;
            }
            else
            {
                // there was an error during recv
                printf("(serverThread)recv failed with error: %d\n", WSAGetLastError());
                closesocket(stParams->serverConnectSocket);
                break;
            }


        } while ((int)responseSize - recievedBytes > 0);

        /*//serverResponse = (fileDataServerResponse*)recvBuff;
        serverResponse->fileParts = (char**)serverResponse->fileParts;
        serverResponse->filePartData = (filePartDataResponse*)serverResponse->filePartData;
        */

        if (responseSize == recievedBytes) {

            //serverResponseSerialized = (fileDataResponseSerialized*)recvBuff;
            serverResponse->responseSize = responseSize;
 
            serverResponse->partsCount = *((long*)recvBuff);
            recvBuff += sizeof(long);
            //serverResponse->partsCount = ntohl(serverResponse->partsCount);
            EnterCriticalSection(&stParams->printCS);
            printf("\nResponse size: %d", serverResponse->responseSize);
            printf("\nPart count: %d", serverResponse->partsCount);
            LeaveCriticalSection(&stParams->printCS);
            filePartDataResponse* savedAddress = NULL;

            serverResponse->filePartData = (filePartDataResponse*)malloc(sizeof(filePartDataResponse) * serverResponse->partsCount);

            for (int i = 0; i < serverResponse->partsCount; i++) {
                //serverResponse->filePartData += i * sizeof(filePartDataResponse);
                //serverResponse->filePartData = (filePartDataResponse*)malloc(1 * sizeof(filePartDataResponse));
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
                    //ovde treba da se stavi null terminator
                    serverResponse->filePartData[i].filePartAddress[serverResponse->filePartData[i].filePartSize] = '\0';
                    recvBuff += serverResponse->filePartData[i].filePartSize;
                }
                else
                {
                    serverResponse->filePartData[i].filePartAddress = (char*)malloc(sizeof(char*));
                    memcpy(serverResponse->filePartData[i].filePartAddress, recvBuff, sizeof(char*));
                    //ovde treba da se stavi null terminator
                    //serverResponse->filePartData[i].filePartAddress[sizeof(char*)] = '\0';
                    recvBuff += sizeof(char*);
                }
                EnterCriticalSection(&stParams->printCS);
                printf("\nFile Part size [%d]: %ld", i, serverResponse->filePartData[i].filePartSize);
                printf("\nRelative address[%d]: %ld", i, serverResponse->filePartData[i].relativeAddress);
                printf("\nIP Address of client that we need to connect[%d]: %s", i, inet_ntoa(serverResponse->filePartData[i].ipClientSocket.sin_addr));
                printf("\nPort of client that we need to connect[%d]: %d", i, serverResponse->filePartData[i].ipClientSocket.sin_port);
                printf("\nPart of message [%d]: %s", i, serverResponse->filePartData[i].filePartAddress);
                printf("\n");
                LeaveCriticalSection(&stParams->printCS);

            }
            
            serverResponse->filePartData = savedAddress;

        }
        //u print buffer pisemo celokupnu poruku
        char* printBuffer = NULL;
        int clientBufferFull = 0;
        int numberOfWrittenBytes = 0;
        char* startPrintBuffer = NULL;
        int startPrintBufferFlag = 0;
        char* endOfPrintBuffer = NULL;

        //free the first recvBuff
        recvBuff = startRecvBuff;
        free(recvBuff);

        //zauzimamo memoriju za ispis celog fajla
        int completeFileSize = 0;
        for (int i = 0; i < serverResponse->partsCount; i++) {
            completeFileSize += serverResponse->filePartData[i].filePartSize;
        }

        printBuffer = (char*)malloc(sizeof(char) * (completeFileSize + 1));

        for (int i = 0; i < serverResponse->partsCount; i++) {
            if (serverResponse->filePartData[i].ipClientSocket.sin_port == 0 &&
                serverResponse->filePartData[i].ipClientSocket.sin_addr.S_un.S_addr == inet_addr("0.0.0.0")) {

                //u ovom delu koda stavljamo onaj deo fajla koji se cuva kod klijenta
                if (!clientBufferFull) {
                    //proveravamo da li je deo fajla veci ili jednak od velicine bafera
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

                if (!startPrintBufferFlag) {
                    startPrintBuffer = printBuffer;
                    startPrintBufferFlag = 1;
                }

                //upisujemo u buffer za ispis celog fajla
                memcpy(printBuffer, serverResponse->filePartData[i].filePartAddress, serverResponse->filePartData[i].filePartSize);
                printBuffer += serverResponse->filePartData[i].filePartSize;
                endOfPrintBuffer = printBuffer;
            }
            else {

                // create a socket
                SOCKET clientConnectSocket = socket(AF_INET,
                    SOCK_STREAM,
                    IPPROTO_TCP);

                if (clientConnectSocket == INVALID_SOCKET)
                {
                    EnterCriticalSection(&stParams->printCS);
                    printf("(serverThread)socket failed with error: %ld\n", WSAGetLastError());
                    LeaveCriticalSection(&stParams->printCS);
                    WSACleanup();
                    return 1;
                }
                
                // create and initialize address structure
                sockaddr_in clientAddress;
                clientAddress.sin_family = AF_INET;
                //const char* buff = inet_ntoa(serverResponse->filePartData[i].ipClientSocket.sin_addr);

                clientAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
                clientAddress.sin_port = htons(serverResponse->filePartData[i].ipClientSocket.sin_port + 1);

                // connect to server specified in serverAddress and socket serverConnectSocket
                if (connect(clientConnectSocket, (SOCKADDR*)&clientAddress, sizeof(clientAddress)) == SOCKET_ERROR)
                {
                    EnterCriticalSection(&stParams->printCS);
                    printf("\nError: %d", WSAGetLastError());
                    printf("(serverThread)Unable to connect to server.\n");
                    LeaveCriticalSection(&stParams->printCS);
                    closesocket(clientConnectSocket);
                    WSACleanup();
                }
                
                /*
                // create and initialize address structure
                sockaddr_in serverAddress;
                serverAddress.sin_family = AF_INET;
                serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
                int* port = (int*)malloc(sizeof(int));
                *port = (int)(serverResponse->filePartData[i].ipClientSocket.sin_port + 1);
                serverAddress.sin_port = htons(*port);
                // connect to server specified in serverAddress and socket serverConnectSocket
                if (connect(clientConnectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
                {
                    printf("Unable to connect to server.\n");
                    closesocket(clientConnectSocket);
                    WSACleanup();
                }
                */
                //prebacujemo u neblokirajuci rezim
                unsigned long mode = 1; //non-blocking mode
                iResult = ioctlsocket(clientConnectSocket, FIONBIO, &mode);
                if (iResult != NO_ERROR)
                    printf("(serverThread)ioctlsocket failed with error: %ld\n", iResult);

                fd_set readfds;

                timeval timeVal;
                timeVal.tv_sec = 1;
                timeVal.tv_usec = 0;

                recvBuff = (char*)malloc(sizeof(char) * serverResponse->filePartData[i].filePartSize + 1);


                do
                {
                    FD_ZERO(&readfds);
                    FD_SET(clientConnectSocket, &readfds);

                    result = select(0, &readfds, NULL, NULL, &timeVal);

                    if (result == 0) {
                        //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
                        //Sleep(1000);
                        continue;
                    }
                    else if (result == SOCKET_ERROR) {
                        // connection was closed gracefully
                        printf("(serverThread)Connection with client closed.\n");
                        closesocket(clientConnectSocket);
                        break;
                    }

                    // Receive data until the client shuts down the connection
                    iResult = recv(clientConnectSocket, recvBuff, serverResponse->filePartData[i].filePartSize, 0);

                    if (iResult > 0)
                    {
                        recvBuff[serverResponse->filePartData[i].filePartSize] = '\0';
                        printf("(serverThread)Message received from client: %s\n", recvBuff);

                        if (!startPrintBufferFlag) {
                            startPrintBuffer = printBuffer;
                            startPrintBufferFlag = 1;
                        }

                        memcpy(printBuffer, recvBuff, serverResponse->filePartData[i].filePartSize);

                        free(recvBuff);
                        printBuffer += serverResponse->filePartData[i].filePartSize;
                        endOfPrintBuffer = printBuffer;

                        FD_CLR(clientConnectSocket, &readfds);
                        break;
                    }
                    else if (iResult == 0)
                    {
                        // connection was closed gracefully
                        printf("(serverThread)Connection with client closed.\n");
                        closesocket(clientConnectSocket);
                        break;
                    }
                    else
                    {
                        // there was an error during recv
                        printf("(serverThread)recv failed with error: %d\n", WSAGetLastError());
                        closesocket(clientConnectSocket);
                        break;
                    }
                } while (1);
                // cleanup
                //free(recvBuff);
                closesocket(clientConnectSocket);

            }
        }
        EnterCriticalSection(&stParams->printCS);
        *endOfPrintBuffer = '\0';
        printf("\nCelokupna poruka: %s\n", startPrintBuffer);
        LeaveCriticalSection(&stParams->printCS);
       
        free(startPrintBuffer);

    }
    // cleanup

    closesocket(stParams->serverConnectSocket);
    WSACleanup();


    return 0;
}
