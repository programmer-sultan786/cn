
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define PORT 8080
#define SIZE 1024

SOCKET client_socket;

DWORD WINAPI receive_messages(LPVOID arg)
{
    char msg[SIZE];
    int n;

    while ((n = recv(client_socket, msg, SIZE - 1, 0)) > 0)
    {
        msg[n] = '\0';
        printf("\n%sYou: ", msg);
        fflush(stdout);
    }

    printf("\nDisconnected from server.\n");
    return 0;
}

int main()
{
    WSADATA wsa;
    struct sockaddr_in server;
    char msg[SIZE];

    printf("Initializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    printf("Creating TCP socket...\n");
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == INVALID_SOCKET)
        goto cleanup;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    printf("Connecting to server at 127.0.0.1:%d...\n", PORT);

    if (connect(client_socket, (struct sockaddr *)&server,
                sizeof(server)) == SOCKET_ERROR)
    {
        printf("Connection failed.\n");
        closesocket(client_socket);
        goto cleanup;
    }

    printf("Connected to server successfully!\n");

    HANDLE t = CreateThread(NULL, 0, receive_messages, NULL, 0, NULL);

    if (!t)
    {
        closesocket(client_socket);
        goto cleanup;
    }

    while (fgets(msg, SIZE, stdin))
    {
        send(client_socket, msg, (int)strlen(msg), 0);

        if (strncmp(msg, "exit", 4) == 0)
            break;

        printf("You: ");
        fflush(stdout);
    }

    shutdown(client_socket, SD_BOTH);
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    closesocket(client_socket);

cleanup:
    WSACleanup();
    return 0;
}
