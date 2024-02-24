#include <iostream>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>

#define N_THREADS 32

int PORT = 6250;

bool checkPrime(const int& n);
void getPrimes(int lowerBound, int upperBound, std::vector<int>& primes);

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

    // Accept a client socket
    SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected.\n";

    // Do something with the clientSocket (send/receive data)

    // send request to client to get n 
    const char* sendData = "Please enter n: ";
    send(clientSocket, sendData, strlen(sendData), 0);

    // Receiving an n 
    int receivedInt;
    int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedInt), sizeof(receivedInt), 0);

    if (bytesReceived > 0) {
        // Convert from network byte order to host byte order
        receivedInt = ntohl(receivedInt);
        std::cout << "Received integer: " << receivedInt << std::endl;
    }
    else if (bytesReceived == 0) {
        std::cout << "Connection closed by client...\n";
    }
    else {
        std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
    }

    // get primes 
    std::vector<int> primes;
    std::vector<std::thread> threads;

    // launch threads 
    for (int i = 0; i < N_THREADS; i++) {
        int division = receivedInt / N_THREADS;
        int lowerBound = division * i + 2;
        int upperBound = receivedInt + 1;

        if (i < N_THREADS - 1) {
            upperBound = division * (i + 1) + 2;
        }

        threads.emplace_back(getPrimes, lowerBound, upperBound, std::ref(primes));
    }

    // wait for threads to finish 
    for (auto& thread : threads) {
        thread.join();
    }

    // send n to server 

    int numPrimes = htonl(primes.size());
    send(clientSocket, reinterpret_cast<char*>(&numPrimes), sizeof(numPrimes), 0);

    // Cleanup
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

bool checkPrime(const int& n) {
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

void getPrimes(int lowerBound, int upperBound, std::vector<int>& primes) {
    for (int currentNum = lowerBound; currentNum < upperBound; currentNum++) {
        if (checkPrime(currentNum)) {
            std::unique_lock<std::mutex> lock(myMutex);
            primes.push_back(currentNum);
            lock.unlock();
        }
    }
}