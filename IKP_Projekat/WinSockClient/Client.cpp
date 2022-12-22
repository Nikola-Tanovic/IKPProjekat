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
    short clientPort;
    CRITICAL_SECTION bufferCS;
    CRITICAL_SECTION printCS;
    char* clientBuff;
}listenThreadParameters;




// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.
bool InitializeWindowsSockets();

DWORD WINAPI serverThreadFunction(LPVOID lpParam);
DWORD WINAPI listenThreadFunction(LPVOID lpParam);

int __cdecl main(int argc, char** argv)
{
    // socket used to communicate with server
    SOCKET connectSocket = INVALID_SOCKET;
    // variable used to store function return value
    int iResult;

    char* recvBuff = (char*)malloc(DEFAULT_BUFLEN);
    

    int result = 0;

    // 
    long initialServerResponse = -1;

    //ovde ce se smestati deo fajla koji se cuva kod klijenta
    char* clientBuffer = NULL;

    
    // message to send
    request requestMessage;

    
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
    connectSocket = socket(AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP);

    if (connectSocket == INVALID_SOCKET)
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
    // connect to server specified in serverAddress and socket connectSocket
    if (connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
    {
        printf("Unable to connect to server.\n");
        closesocket(connectSocket);
        WSACleanup();
    }
    //moramo da dobavimo od servera podatke o klijent soketu da bi znali u listenThreadu na sta da bindujemo listen socket
    sockaddr_in clientAddr;
    
    iResult = recv(connectSocket, recvBuff, DEFAULT_BUFLEN, 0);
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
        closesocket(connectSocket);
    }
    else
    {
        // there was an error during recv
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(connectSocket);
    }
    //ovde pozivamo listenThread koji slusa zahteve od drugih klijenata
    //clientSocket, bufferCS, printCS, clientBuff, 

    listenThreadParameters* ltParams = (listenThreadParameters*)malloc(sizeof(listenThreadParameters));
    ltParams->bufferCS = bufferCS;
    ltParams->printCS = printCS;
    ltParams->clientPort = ntohs(clientAddr.sin_port);
    ltParams->clientBuff = clientBuffer;

    DWORD listenThreadID;
    HANDLE listenThread = CreateThread(NULL, 0, &listenThreadFunction, ltParams, 0, &listenThreadID);




    //odavde ide thread za komunikaciju sa serverom
    //prebacujemo u neblokirajuci rezim
    unsigned long mode = 1; //non-blocking mode
    iResult = ioctlsocket(connectSocket, FIONBIO, &mode);
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
        FD_SET(connectSocket, &readfds);
        int result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
            //Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            printf("Connection with server closed.\n");
            closesocket(connectSocket);
            break;
        }

        iResult = recv(connectSocket, (char*)&initialServerResponse, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            initialServerResponse = ntohl((long)initialServerResponse);
            printf("Broj fajlova raspolozivih na serveru je: %ld\n", initialServerResponse);
            break;
        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
            printf("Connection with client closed.\n");
            closesocket(connectSocket);
            break;
        }
        else
        {
            // there was an error during recv
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(connectSocket);
            break;
        }
    } while (initialServerResponse == -1);

    while (1) {

        FD_ZERO(&writefds);

        FD_SET(connectSocket, &writefds);

        result = select(0, NULL, &writefds, NULL, &timeVal);

        if (result == 0) {
            printf("Vreme za cekanje je isteklo (select pre send funkcije - Klijent )\n");
            Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            printf("Connection with client closed.\n");
            closesocket(connectSocket);
            break;
        }

        long fileId = -1;
        do {
            printf("Unesite id fajla koji zelite: ");
            scanf("%ld", &fileId);
            requestMessage.fileId = htonl(fileId);
        } while (fileId >= initialServerResponse || fileId < 0);


        long bufferSize = -1;
        printf("\nUnesite velicinu fajla koju zelite da smestite kod sebe: ");
        scanf("%ld", &bufferSize);
        requestMessage.bufferSize = htonl(bufferSize);

        clientBuffer = (char*)malloc(sizeof(char) * bufferSize);


        // Send an prepared message with null terminator included
        iResult = send(connectSocket, (char*)&requestMessage, (int)sizeof(request), 0);

        if (iResult == SOCKET_ERROR)
        {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(connectSocket);
            WSACleanup();
            return 1;
        }

        printf("Bytes Sent: %ld\n", iResult);

        FD_CLR(connectSocket, &writefds);

        fileDataResponse* serverResponse = (fileDataResponse*)malloc(sizeof(fileDataResponse));
        //short recievedBytes = 0;

        int recievedBytes = 0;
        int responseSizeRecieved = 0;
        short responseSize = 1;

     
        

        do{
            FD_ZERO(&readfds);
            FD_SET(connectSocket, &readfds);
            int result = select(0, &readfds, NULL, NULL, &timeVal);

            if (result == 0) {
                //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
                //Sleep(1000);
                continue;
            }
            else if (result == SOCKET_ERROR) {
                printf("Connection with server closed.\n");
                closesocket(connectSocket);
                break;
            }
            
            //moze da se desi greska ako saljemo mnogo bajtova da bude prepunjen ovaj default buflen
            iResult = recv(connectSocket, recvBuff + recievedBytes, DEFAULT_BUFLEN, 0);
            if (iResult > 0) {

                if (responseSizeRecieved == 0){
                    //recvBuff[iResult] = '\0';
                    responseSize = ntohs(((fileDataResponse*)recvBuff)->responseSize);
                    responseSizeRecieved = 1;
                }
                
               
                recievedBytes += iResult;
            }
            else if (iResult == 0)
            {
                // connection was closed gracefully
                //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
                printf("Connection with client closed.\n");
                closesocket(connectSocket);
                break;
            }
            else
            {
                // there was an error during recv
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(connectSocket);
                break;
            }
         
            FD_CLR(connectSocket, &readfds);
        } while ((int)responseSize - recievedBytes > 0);

        /*//serverResponse = (fileDataServerResponse*)recvBuff;
        serverResponse->fileParts = (char**)serverResponse->fileParts;
        serverResponse->filePartData = (filePartDataResponse*)serverResponse->filePartData;
        */

        if (responseSize == recievedBytes) {

            serverResponse = (fileDataResponse*)recvBuff;
            serverResponse->responseSize = ntohs(serverResponse->responseSize);
            serverResponse->partsCount = ntohl(serverResponse->partsCount);
            EnterCriticalSection(&printCS);
            printf("\nResponse size: %d", serverResponse->responseSize);
            printf("\nPart count: %d", serverResponse->partsCount);
            LeaveCriticalSection(&printCS);
   
            for (int i = 0; i < serverResponse->partsCount; i++) {
                serverResponse->filePartData += i * sizeof(filePartDataResponse);
                serverResponse->filePartData = (filePartDataResponse*)malloc(1 * sizeof(filePartDataResponse));
                serverResponse->filePartData[i].filePartSize = ntohl(serverResponse->filePartData[i].filePartSize);


                serverResponse->filePartData[i].filePartAddress = (char*)malloc(serverResponse->filePartData[i].filePartSize * sizeof(char));

                serverResponse->filePartData[i].relativeAddress = ntohl(serverResponse->filePartData[i].relativeAddress);
                serverResponse->filePartData[i].ipClientSocket.sin_port = ntohs(serverResponse->filePartData[i].ipClientSocket.sin_port);
                
                EnterCriticalSection(&printCS);
                printf("\nFile Part size [%d]: %ld", i, serverResponse->filePartData[i].filePartSize);
                printf("\nRelative address[%d]: %ld", i, serverResponse->filePartData[i].relativeAddress);
                printf("\nIP Address of client that we need to connect[%d]: %s", i, inet_ntoa(serverResponse->filePartData[i].ipClientSocket.sin_addr));
                printf("\nPort of client that we need to connect[%d]: %d", i, serverResponse->filePartData[i].ipClientSocket.sin_port);
                printf("\nPart of message [%d]: %s", serverResponse->filePartData[i].filePartAddress);
                LeaveCriticalSection(&printCS);

            }



        }

        //ubacili smo deo poruke kod nas na serveru
        



        free(serverResponse);
    }
    // cleanup
    
    closesocket(connectSocket);
    WSACleanup();


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

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, (PCSTR)ltParams->clientPort, &hints, &resultingAddress);
    if (iResult != 0)
    {
        EnterCriticalSection(&ltParams->printCS);
        printf("getaddrinfo failed with error: %d\n", iResult);
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
        printf("socket failed with error: %ld\n", WSAGetLastError());
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
        printf("bind failed with error: %d\n", WSAGetLastError());
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
        printf("listen failed with error: %d\n", WSAGetLastError());
        LeaveCriticalSection(&ltParams->printCS);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    EnterCriticalSection(&ltParams->printCS);
    printf("Listen client socket initialized, waiting for other clients.\n");
    LeaveCriticalSection(&ltParams->printCS);

    iResult = ioctlsocket(listenSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
        EnterCriticalSection(&ltParams->printCS);
        printf("ioctlsocket failed with error: %ld\n", iResult);
        LeaveCriticalSection(&ltParams->printCS);

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
            printf("Connection with client closed.\n");
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
            printf("accept failed with error: %d\n", WSAGetLastError());
            LeaveCriticalSection(&ltParams->printCS);
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        EnterCriticalSection(&ltParams->bufferCS);
        char* messageToSend = ltParams->clientBuff;
        LeaveCriticalSection(&ltParams->bufferCS);

        iResult = send(acceptedSocket, messageToSend, (int)strlen(messageToSend) + 1, 0);
        if (iResult == SOCKET_ERROR)
        {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(acceptedSocket);
            WSACleanup();
            return 1;
        }

        printf("Bytes Sent: %ld\n", iResult);

    } while (1);

    // shutdown the connection since we're done
    iResult = shutdown(acceptedSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(acceptedSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(listenSocket);
    closesocket(acceptedSocket);
    WSACleanup();

    return 0;
}
