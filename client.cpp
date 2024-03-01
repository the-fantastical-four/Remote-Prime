#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>  // Include for InetPton function
#include <tchar.h>
#include <chrono>  // Include the chrono library


#pragma comment(lib, "ws2_32.lib")  // Link against the Winsock library

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

    // TODO: prompt user for a start point and end point 
    int startPoint;
    int endPoint; 

    std::cout << "Input start: "; 
    std::cin >> startPoint; 

    std::cout << "Input end: "; 
    std::cin >> endPoint; 

    int startData = htonl(startPoint);
    int endData = htonl(endPoint); 

    // Start timing before sending data
    auto start = std::chrono::high_resolution_clock::now();

    send(clientSocket, reinterpret_cast<char*>(&startData), sizeof(startData), 0);
    send(clientSocket, reinterpret_cast<char*>(&endData), sizeof(endData), 0); 

    // wait for response from server 
    int bytesReceived; 
    int receivedNumPrime;
    do {
        bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedNumPrime), sizeof(receivedNumPrime), 0);

        if (bytesReceived > 0) {
            receivedNumPrime = ntohl(receivedNumPrime); 
            std::cout << "Number of primes: " << receivedNumPrime << std::endl;

            // Stop timing after receiving response
            auto end = std::chrono::high_resolution_clock::now();

            // Calculate the duration
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            // Output the duration in milliseconds
            std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
        }
        else if (bytesReceived == 0) { // if server socket closes 
            std::cout << "Connection closing...\n";
        }
        else {
            std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
        }

    } while (bytesReceived > 0); 

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}