#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>  // Include for InetPton function
#include <tchar.h>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <random>

#pragma comment(lib, "ws2_32.lib")  // Link against the Winsock library

#define N_THREADS 4
int PORT = 6251;

bool checkPrime(const int& n);
void getPrimes(int lowerBound, int upperBound, std::vector<int>& numList, std::vector<int>& primes);
int launchThreads(std::vector<int> numList);

std::mutex myMutex;

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock.\n";
        return 1;
    }

    // Create a socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
        WSACleanup();
        return 1;
    }

    // Bind the socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT); // You can choose any port

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << PORT << "...\n";

    // Accept a connection
    SOCKET mainServerSocket = accept(serverSocket, nullptr, nullptr);
    if (mainServerSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to Main Server" << std::endl;

    // Receive the size of the list first

    // Receive the size of the vector
    int32_t receivedSize;
    char sizeBuffer[sizeof(receivedSize)];
    recv(mainServerSocket, sizeBuffer, sizeof(receivedSize), 0);
    memcpy(&receivedSize, sizeBuffer, sizeof(receivedSize));

    // Allocate space for the vector data and receive it
    std::vector<int> receivedVector(receivedSize);
    char* recvDataBuffer = reinterpret_cast<char*>(receivedVector.data());
    recv(mainServerSocket, recvDataBuffer, sizeof(int) * receivedSize, 0);
   
    // Process the received list of numbers
    int numPrimes = launchThreads(receivedVector);
    std::cout << "Total primes found: " << numPrimes << std::endl;

    int totalPrimes = htonl(numPrimes);
    send(mainServerSocket, reinterpret_cast<char*>(&totalPrimes), sizeof(totalPrimes), 0);

    // Cleanup
    closesocket(mainServerSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

int launchThreads(std::vector<int> numList) {
    // get primes 
    std::vector<int> primes;
    std::vector<std::thread> threads;

    // launch threads 
    for (int i = 0; i < N_THREADS; i++) {
        int division = numList.size() / N_THREADS;
        int lowerBound = division * i;
        int upperBound = numList.size();

        if (i < N_THREADS - 1) {
            upperBound = division * (i + 1);
        }

        threads.emplace_back(getPrimes, lowerBound, upperBound, std::ref(numList), std::ref(primes));
    }

    // wait for threads to finish 
    for (auto& thread : threads) {
        thread.join();
    }

    return primes.size();
}

bool checkPrime(const int& n) {
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

void getPrimes(int lowerBound, int upperBound, std::vector<int>& numList, std::vector<int>& primes) {
    for (int i = lowerBound; i < upperBound; i++) {
        if (checkPrime(numList[i])) {
            std::unique_lock<std::mutex> lock(myMutex);
            primes.push_back(i);
            lock.unlock();
        }
    }
}