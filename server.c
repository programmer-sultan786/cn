#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define SIZE 1024

typedef struct {
    SOCKET sock;
    int id;
} Client;

Client clients[MAX_CLIENTS];
int count = 0, id = 1;
CRITICAL_SECTION cs;

void broadcast(char *msg, SOCKET sender)
{
    EnterCriticalSection(&cs);
    for (int i = 0; i < count; i++)
        if (clients[i].sock != sender)
            send(clients[i].sock, msg, strlen(msg), 0);
    LeaveCriticalSection(&cs);
}

DWORD WINAPI server_chat(LPVOID arg)
{
    char msg[SIZE], temp[SIZE + 20];

    while (fgets(msg, SIZE, stdin))
    {
        snprintf(temp, sizeof(temp), "Server: %s", msg);
        broadcast(temp, INVALID_SOCKET);
    }
    return 0;
}

DWORD WINAPI client_chat(LPVOID arg)
{
    Client *c = (Client *)arg;
    SOCKET s = c->sock;
    int cid = c->id, n;
    char msg[SIZE], temp[SIZE + 30];

    while ((n = recv(s, msg, SIZE - 1, 0)) > 0)
    {
        msg[n] = '\0';
        snprintf(temp, sizeof(temp), "Client %d: %s", cid, msg);
        printf("%s", temp);
        broadcast(temp, s);
        if (strncmp(msg, "exit", 4) == 0) break;
    }

    EnterCriticalSection(&cs);
    for (int i = 0; i < count; i++)
        if (clients[i].sock == s)
        {
            clients[i] = clients[--count];
            break;
        }
    LeaveCriticalSection(&cs);

    printf("Client %d disconnected.\n", cid);
    closesocket(s);
    free(c);
    return 0;
}

int main()
{
    WSADATA wsa;
    SOCKET server, s;
    struct sockaddr_in addr, client;
    int len = sizeof(client);

    printf("Initializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    printf("Creating socket...\n");
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) goto end;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    printf("Binding to port %d...\n", PORT);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("Binding failed.\n");
        goto close_server;
    }

    printf("Listening for clients...\n");
    if (listen(server, MAX_CLIENTS) == SOCKET_ERROR)
    {
        printf("Listen failed.\n");
        goto close_server;
    }

    InitializeCriticalSection(&cs);
    printf("Server is ready. Type messages to chat.\n");
    CreateThread(NULL, 0, server_chat, NULL, 0, NULL);

    while (1)
    {
        s = accept(server, (struct sockaddr *)&client, &len);
        if (s == INVALID_SOCKET) continue;

        EnterCriticalSection(&cs);

        if (count == MAX_CLIENTS)
        {
            LeaveCriticalSection(&cs);
            send(s, "Server is full.\n", 16, 0);
            closesocket(s);
            continue;
        }

        Client *c = malloc(sizeof(Client));
        if (!c)
        {
            LeaveCriticalSection(&cs);
            closesocket(s);
            continue;
        }

        *c = (Client){s, id++};
        clients[count++] = *c;
        printf("Client %d connected.\n", c->id);
        LeaveCriticalSection(&cs);

        char welcome[50];
        sprintf(welcome, "You are Client %d.\n", c->id);
        send(s, welcome, strlen(welcome), 0);

        HANDLE t = CreateThread(NULL, 0, client_chat, c, 0, NULL);

        if (t) CloseHandle(t);
        else
        {
            EnterCriticalSection(&cs);
            for (int i = 0; i < count; i++)
                if (clients[i].sock == s)
                {
                    clients[i] = clients[--count];
                    break;
                }
            LeaveCriticalSection(&cs);
            closesocket(s);
            free(c);
        }
    }

    DeleteCriticalSection(&cs);

close_server:
    closesocket(server);
end:
    WSACleanup();
    return 0;
}