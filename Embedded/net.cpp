#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <map>

using namespace std;

struct MySock {
    SOCKET f;
};
        
HANDLE mainThread;
std::map<int, char *> receivedLines;
int running;

#define MAX_CLIENTS 10

// main thread
DWORD WINAPI listenLoop(LPVOID LParam) {
    
    long rc;
    SOCKET acceptSocket; 
    SOCKADDR_IN addr;
    char buf[1024];
    
    // zusätzliche Variabeln
    fd_set fdSet;
    SOCKET clients[MAX_CLIENTS];
    int i;
  
    // Winsock starten
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 0), &wsa);

    // Socket erstellen
    acceptSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(acceptSocket == INVALID_SOCKET) 
        return 1;
  
    // Socket binden
    memset(&addr, 0, sizeof(SOCKADDR_IN));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;
    rc = bind(acceptSocket, (SOCKADDR *)&addr, sizeof(SOCKADDR_IN));
    if(rc == SOCKET_ERROR) return 1;
  
    // In den listen Modus
    rc = listen(acceptSocket, MAX_CLIENTS);
    if(rc == SOCKET_ERROR) return 1;
  
    // reset all peers
    for(i = 0; i < MAX_CLIENTS; i++) 
        clients[i] = INVALID_SOCKET;  
    
    running = 1;
    
    while (running) {
    
        FD_ZERO(&fdSet);
        FD_SET(acceptSocket, &fdSet);
    
        for (i = 0; i < MAX_CLIENTS; i++) 
            if (clients[i] != INVALID_SOCKET)   
                FD_SET(clients[i], & fdSet);
    
        rc = select(0, & fdSet, NULL, NULL, NULL);
        
        if (rc == SOCKET_ERROR) 
            return 1;
    
        try {
        
        if(FD_ISSET(acceptSocket, &fdSet)) {
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == INVALID_SOCKET) {
                      clients[i] = accept(acceptSocket, NULL, NULL);
                      //printf("Neuen Client angenommen (%d)\n", i);
                      break;
                }
            }
        }
      
        // prüfen wlecher client sockets im fd_set sind
        for (i = 0; i < MAX_CLIENTS; i++) {
        
            if (clients[i] == INVALID_SOCKET)
                continue;
            
            if (FD_ISSET(clients[i], &fdSet)) {
            
                rc = recv(clients[i], buf, 1024, 0);
                
                if (rc == 0 || rc == SOCKET_ERROR) {
                    
                    //printf("Client %d hat die Verbindung geschlossen\n", i);
                    closesocket(clients[i]);
                    clients[i] = INVALID_SOCKET;
                    
                } 
                
                else {
                
                    buf[rc] = '\0';
                    //printf("Client %d hat folgendes gesandt: %s\n", i, buf);
                    //stringstream ss; ss << buf << "\n";
                    int currSize = receivedLines.size();
                    receivedLines[currSize] = buf; //(char *)ss.str().c_str();
                    //sprintf(buf2, "Du mich auch %s\n", buf);
                    //send(clients[i], buf2, (int) strlen(buf2), 0);
                    
                }
                
            }
            
        }
        
        } catch(...) { printf("catched"); }
      
    }
    
}

extern "C" {

    // constructor
    // the constructor tells jail something about this libraríes exported functions
    // todo
    __declspec(dllexport) char *__construct() {    
        char *tmp = (char *)malloc(128); 
        running = 0;       
        strcpy(tmp, "opensock_nvv;listensock_nv;queue_nv;receivequeue_nv;closesock_nvv;");
        return tmp;                              
    }

    __declspec(dllexport) void* opensock(void *data) {            
        int port = std::stoi((char *)data); 
        mainThread = CreateThread(0, 0, listenLoop, (void *)port, 0, NULL);
        char *ret = (char *)std::to_string(1).c_str();
        running = 1;                    
        return (void *)ret;
    } 

    __declspec(dllexport) void* listensock() {
        char *ret = (char *)malloc(3);
        sprintf(ret, "%d", running);        
        return (void *)ret;        
    }

    __declspec(dllexport) void* queue() {
        int i = receivedLines.size();
        return (void *)std::to_string(i).c_str();        
    }

    __declspec(dllexport) void* receivequeue() {                
        printf("receive called");
        int i = 0;
        std::stringstream ss;
        for(auto it = receivedLines.begin(); it != receivedLines.end(); it++) {
            ss << it->second; 
            i++;   
        }
        receivedLines.clear();
        std::string full = ss.str();
        char *ret = (char *)malloc(strlen(full.c_str() + 1));
        strcpy(ret, (char *)full.c_str());
        ret[strlen(full.c_str())] = '\0';
        printf("receive done with '%s'.", ret);
        return (void *)ret;         
    }

    __declspec(dllexport) void* closesock(void *data) { 
        running = 0;
        if(!TerminateThread(mainThread, 0)) { }
        char *ret = (char *)malloc(3);
        sprintf(ret, "%d", running);
        char *res;
        strcpy(res, ret);
        free(ret);
        return (void *)res;
    }

}

