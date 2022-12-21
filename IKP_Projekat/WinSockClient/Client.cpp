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

// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.
bool InitializeWindowsSockets();


typedef struct fileDataServerResponse {
    short responseSize;
    long partsCount;
    char** fileParts;
    filePartDataResponse* filePartData;
}fileDataServerResponse;


int __cdecl main(int argc, char** argv)
{
    // socket used to communicate with server
    SOCKET connectSocket = INVALID_SOCKET;
    // variable used to store function return value
    int iResult;

    char* recvBuff = NULL;


    int result = 0;

    // 
    long initialServerResponse = -1;

    //ovde ce se smestati deo fajla koji se cuva kod klijenta
    char* clientBuffer = NULL;

    // message to send
    request requestMessage;

    
    CRITICAL_SECTION printCS;



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

        fileDataServerResponse* serverResponse = (fileDataServerResponse*)malloc(sizeof(fileDataServerResponse));
        //short recievedBytes = 0;

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
            iResult = recv(connectSocket, recvBuff /*+ recievedBytes*/, DEFAULT_BUFLEN, 0);
            if (iResult > 0) {
                serverResponse = (fileDataServerResponse*)recvBuff;
                serverResponse->responseSize = ntohs(serverResponse->responseSize);
                serverResponse->partsCount = ntohl(serverResponse->partsCount);

                for (int i = 0; i < requestMessage.bufferSize; i++) {
                    clientBuffer[i] = serverResponse->fileParts[0][i];
                }
                clientBuffer[requestMessage.bufferSize] = '\0';

                EnterCriticalSection(&printCS);
                printf("\nPoruka od servera: %s", serverResponse->fileParts[0]);
                LeaveCriticalSection(&printCS);

                //recievedBytes += iResult;
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
        } while (/*serverResponse->responseSize - recievedBytes > 0*/ 1);

        /*//serverResponse = (fileDataServerResponse*)recvBuff;
        serverResponse->fileParts = (char**)serverResponse->fileParts;
        serverResponse->filePartData = (filePartDataResponse*)serverResponse->filePartData;
        */

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
