
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "threadList.h";


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"

bool InitializeWindowsSockets();

typedef struct threadParameters {
    SOCKET acceptedSocket;
    sockaddr_in clientAddr;
    CRITICAL_SECTION cs;
    DWORD threadId;
    DWORD* returnValue;
}threadParameters;

DWORD WINAPI clientThreadFunction(LPVOID lpParam) {

    char recvBuff[DEFAULT_BUFLEN];
    unsigned long mode = 1;

    threadParameters tParameters = *(threadParameters*)lpParam;

    int iResult = ioctlsocket(tParameters.acceptedSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);

    fd_set readfds;

    FD_ZERO(&readfds);

    FD_SET(tParameters.acceptedSocket, &readfds);

    timeval timeVal;
    timeVal.tv_sec = 1;
    timeVal.tv_usec = 0;

    printf("Konektovan novi klijent\n");

    do
    {
        FD_ZERO(&readfds);
        FD_SET(tParameters.acceptedSocket, &readfds);

        int result = select(0, &readfds, NULL, NULL, &timeVal);

        if (result == 0) {
            //printf("Vreme za cekanje je isteklo (select pre recviver funkcije)\n");
            //Sleep(1000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            printf("Connection with client closed(bababab).\n");
            closesocket(tParameters.acceptedSocket);
            break;
        }

        // Receive data until the client shuts down the connection
        iResult = recv(tParameters.acceptedSocket, recvBuff, DEFAULT_BUFLEN, 0);
        if (iResult > 0)
        {
            EnterCriticalSection(&(tParameters.cs));
            printf("Client: IPAddress: %s Port: %d\n", inet_ntoa(tParameters.clientAddr.sin_addr), ntohs(tParameters.clientAddr.sin_port));
            printf("Thread ID: %d\n", tParameters.threadId);
            printf("Message received from client: %s.\n", recvBuff);
            FD_CLR(tParameters.acceptedSocket, &readfds);
            LeaveCriticalSection(&(tParameters.cs));
        }
        else if (iResult == 0)
        {
            // connection was closed gracefully
            //da li je ovde klijent gracefully zatvorio i kada se ulazi u ovaj deo koda
            printf("Connection with client closed.\n");
            closesocket(tParameters.acceptedSocket);
        }
        else
        {
            // there was an error during recv
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(tParameters.acceptedSocket);
        }
    } while (1);

    return 0;
    // here is where server shutdown loguc could be placed
}


int  main(void)
{
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

    //creating and initializing critical section 
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    //critical section that controls access to threadList
    CRITICAL_SECTION threadListCS;
    InitializeCriticalSection(&threadListCS);


    //initializing list of threads
    threadNode* head = NULL;


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
            //printf("Vreme za cekanje je isteklo (select pre accept funkcije - Server ) \n");
            //Sleep(3000);
            continue;
        }
        else if (result == SOCKET_ERROR) {
            // connection was closed gracefully
            printf("Connection with client closed.\n");
            closesocket(acceptedSocket);
            break;
        }
        else if (FD_ISSET(listenSocket, &readfds)) {
            // Wait for clients and accept client connections.
            // Returning value is acceptedSocket used for further
            // Client<->Server communication. This version of
            // server will handle only one client.
       
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(struct sockaddr_in);

            acceptedSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);

            if (acceptedSocket == INVALID_SOCKET)
            {
                printf("accept failed with error: %d\n", WSAGetLastError());
                closesocket(listenSocket);
                WSACleanup();
                return 1;
            }

            //u tread stavljamo sve ovo ispod ovog komentara, kada prodje accept treba da 
            //napravimo novu nit, u kojoj cemo pozvati ove recieve funkcije

            //cuvamo povratnu vrednost niti
            DWORD threadReturnValue = -1;
            
           
            threadParameters tParameters;
            tParameters.acceptedSocket = acceptedSocket;
            tParameters.clientAddr = clientAddr;
            tParameters.cs = cs;
            tParameters.threadId = 0;
            tParameters.returnValue = &threadReturnValue;

            HANDLE thread = CreateThread(NULL, 0, &clientThreadFunction, &tParameters, 0, &(tParameters.threadId));
           
            //postaviti pitanje vezano za ovo
            //da li moze da se desi da se ne napuni tParameters.threadId a da se udje u ovu kriticnu sekciju?
            EnterCriticalSection(&threadListCS);
            printf("Thread ID(unutar main funkcije servera): %d\n", tParameters.threadId);
            threadNode* tn = createNewThreadNode(thread, tParameters.threadId);
            insertAtHead(&head, tn);
            LeaveCriticalSection(&threadListCS);


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
