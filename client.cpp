#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>  // Include for InetPton function
#include <tchar.h>

#pragma comment(lib, "ws2_32.lib")  // Link against the Winsock library

/*
int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock.\n";
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
        WSACleanup();
        return 1;
    }

    // Connect to the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    if (InetPton(AF_INET, _T("127.0.0.1"), &(serverAddr.sin_addr)) != 1) {  // Use InetPton
        std::cerr << "Invalid address.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    serverAddr.sin_port = htons(6250); // Server port

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
*/