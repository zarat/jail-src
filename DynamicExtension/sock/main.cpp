#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#include <vector>
#include "Jail.h"

typedef struct {
    SOCKET socket;
    char username[20];
} ClientInfo;

std::vector<ClientInfo> clients;
SOCKET serverSocket = INVALID_SOCKET;

std::string getIPAddress(const ClientInfo& clientInfo) {
    
	sockaddr_in addr;
    int addrLen = sizeof(addr);
	
	std::string result = "";
    if (getpeername(clientInfo.socket, (sockaddr*)&addr, &addrLen) == 0) {
		result = inet_ntoa(addr.sin_addr);
    }
	return result;
	
}

extern "C" {

    // Start server
    __declspec(dllexport) void startServer(JAIL::JObject *c, void *data) {
        int port = c->getParameter("port")->getInt();
        
        // Initialisiere Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("Fehler bei WSAStartup\n");
            return;
        }

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            printf("Fehler beim Erstellen des Server-Sockets\n");
            WSACleanup();
            return;
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            printf("Fehler beim Binden des Sockets\n");
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        if (listen(serverSocket, 5) == SOCKET_ERROR) {
            printf("Fehler beim Starten des Listens\n");
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        //printf("Server läuft auf Port %d\n", port);
    }

    // Accept new client
    __declspec(dllexport) void acceptClient(JAIL::JObject *c, void *data) {
        if (serverSocket == INVALID_SOCKET) {
            return;  // Kein Server läuft
        }

        SOCKET clientSocket;
        struct sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            printf("Fehler beim Akzeptieren einer Verbindung\n");
            return;
        }

        // Neue Client-Informationen
        ClientInfo clientInfo;
        clientInfo.socket = clientSocket;
        strcpy(clientInfo.username, "anonymous");  // Hier könnte eine echte Authentifizierung erfolgen

        clients.push_back(clientInfo);

        // Client-Index zurückgeben
        int newIndex = clients.size() - 1;
        c->getReturnVar()->setInt(newIndex);
    }

    // Send a message
    __declspec(dllexport) void sendMessage(JAIL::JObject *c, void *data) {
        int clientIndex = c->getParameter("clientIndex")->getInt();
        std::string message = c->getParameter("message")->getString();

        if (clientIndex < 0 || clientIndex >= clients.size()) {
            return;  // Ungültiger Client-Index
        }

        ClientInfo &client = clients[clientIndex];
        if (send(client.socket, message.c_str(), message.length(), 0) == SOCKET_ERROR) {
            printf("Fehler beim Senden an den Client\n");
        }
    }
	
	// Receive a message 
	__declspec(dllexport) void receiveMessage(JAIL::JObject *c, void *data) {
        int clientIndex = c->getParameter("clientIndex")->getInt();
        int bufferSize = c->getParameter("bufferSize")->getInt();
        
        if (clientIndex < 0 || clientIndex >= clients.size()) {
            return;  // Ungültiger Client-Index
        }

        ClientInfo &client = clients[clientIndex];

        // Puffer für die empfangene Nachricht
        char buffer[bufferSize];
        int recvSize = recv(client.socket, buffer, bufferSize, 0);

        if (recvSize == SOCKET_ERROR || recvSize == 0) {
            c->getReturnVar()->setInt(-1); //printf("Fehler beim Empfangen oder Verbindung zum Client verloren\n");
            return;
        }

        // Empfangenes Nachricht in ein JObject zurückgeben
        std::string receivedMessage(buffer, recvSize);
        c->getReturnVar()->setString(receivedMessage.c_str());
    }

    // Close connection
    __declspec(dllexport) void closeClient(JAIL::JObject *c, void *data) {
        int clientIndex = c->getParameter("clientIndex")->getInt();

        if (clientIndex < 0 || clientIndex >= clients.size()) {
            return;  // Ungültiger Client-Index
        }

        ClientInfo &client = clients[clientIndex];
        closesocket(client.socket);

        // Client aus der Liste entfernen
        clients.erase(clients.begin() + clientIndex);
    }

    // Stop server
    __declspec(dllexport) void stopServer(JAIL::JObject *c, void *data) {
        
		if (serverSocket != INVALID_SOCKET) {
            closesocket(serverSocket);
            serverSocket = INVALID_SOCKET;
        }
        
        WSACleanup();
    }

	// Get ip address of client socket
	_declspec(dllexport) void getClientIp(JAIL::JObject *c, void *data) {
	
		int index = c->getParameter("client")->getInt();
		
		ClientInfo& client = clients[index]; // Zugriff auf den ersten Client
        std::string ipAddress = getIPAddress(client); // IP-Adresse ermitteln
        //printf("Benutzername: %s\n", client.username);
        //printf("IP-Adresse: %s\n", ipAddress.c_str());
		c->getReturnVar()->setString(ipAddress);
		
	}
	
	_declspec(dllexport) void test(JAIL::JObject *c, void *data) {
		
		JAIL::JObject *obj = c->getParameter("obj");
		c->setReturnVar(obj);
		
	}
	
    // Register
    __declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
        interpreter->addNative("function Socket.startServer(port)", startServer, 0);
        interpreter->addNative("function Socket.acceptClient()", acceptClient, 0);
        interpreter->addNative("function Socket.sendMessage(clientIndex, message)", sendMessage, 0);
		interpreter->addNative("function Socket.receiveMessage(clientIndex, bufferSize)", receiveMessage, 0);
        interpreter->addNative("function Socket.closeClient(clientIndex)", closeClient, 0);
        interpreter->addNative("function Socket.stopServer()", stopServer, 0);
		interpreter->addNative("function Socket.getIp(client)", getClientIp, 0);
		interpreter->addNative("function Socket.test(obj)", test, 0);
		
    }

}
