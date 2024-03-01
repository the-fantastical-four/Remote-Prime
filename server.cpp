#include <iostream>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <random>

#define N_THREADS 4

int PORT = 6250;
int SERVER1_PORT = 6251;
int SERVER2_PORT = 6252;

bool checkPrime(const int& n);
void getPrimes(int lowerBound, int upperBound, std::vector<int>& primes);
int launchThreads(int start, int end);
void listenForResponse(SOCKET server, int& numPrimes);

std::mutex myMutex;
std::mutex numPrimesMutex; 

int main() {
    bool server1Connect = false; 
    bool server2Connect = false; 

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

    // Define the addresses for the two other servers
    sockaddr_in server1Addr;
    server1Addr.sin_family = AF_INET;
    server1Addr.sin_port = htons(SERVER1_PORT); // Port of the first other server
    server1Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP of the first other server

    sockaddr_in server2Addr;
    server2Addr.sin_family = AF_INET;
    server2Addr.sin_port = htons(SERVER2_PORT); // Port of the second other server
    server2Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP of the second other server

    // Create sockets for connecting to the other servers
    SOCKET server1Socket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKET server2Socket = socket(AF_INET, SOCK_STREAM, 0);

    int serverConnections = 1; // inclusive of the main server 

    // Connect to the first other server
    if (connect(server1Socket, (sockaddr*)&server1Addr, sizeof(server1Addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server 1.\n";
    }
    else {
        std::cout << "Connected to server 1.\n";
        serverConnections++; 
        server1Connect = true; 
    }

    // Connect to the second other server
    if (connect(server2Socket, (sockaddr*)&server2Addr, sizeof(server2Addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server 2.\n";
    }
    else {
        std::cout << "Connected to server 2.\n";
        serverConnections++; 
        server2Connect = true; 
    }

    // TODO: change to receive start and end point 

    // Receiving an n from client
    int receivedStart;
    int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedStart), sizeof(receivedStart), 0);

    if (bytesReceived > 0) {
        // Convert from network byte order to host byte order
        receivedStart = ntohl(receivedStart);
        std::cout << "Received integer: " << receivedStart << std::endl;
    }
    else if (bytesReceived == 0) {
        std::cout << "Connection closed by client...\n";
    }
    else {
        std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
    }

    // Receiving an n from client
    int receivedEnd;
    int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedEnd), sizeof(receivedEnd), 0);

    if (bytesReceived > 0) {
        // Convert from network byte order to host byte order
        receivedEnd = ntohl(receivedEnd);
        std::cout << "Received integer: " << receivedEnd << std::endl;
    }
    else if (bytesReceived == 0) {
        std::cout << "Connection closed by client...\n";
    }
    else {
        std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
    }

    std::vector<std::pair<int, int>> indices; 

    for (int i = receivedStart; i < serverConnections; i++) {
        int division = receivedEnd / serverConnections; 
        int start = division * i + 1;
        int end = receivedEnd;

        if (i < serverConnections - 1) {
            end = division * (i + receivedStart); 
        }
        indices.emplace_back(std::make_pair(start, end)); 
    }

    // sending messages to sub servers  
    for (int i = 1; i < serverConnections; i++) { // start at 1 to exclude slice of main server 

        int start = indices[i].first; 
        int end = indices[i].second;

        int startData = htonl(start);
        int endData = htonl(end); 

        int socket = (server1Connect && i == 1) ? server1Socket : server2Socket; 

        int startSent = send(socket, reinterpret_cast<char*>(&startData), sizeof(startData), 0);
        int endSent = send(socket, reinterpret_cast<char*>(&endData), sizeof(endData), 0); 
        
        // check for errors 
        if (startSent == SOCKET_ERROR || endSent == SOCKET_ERROR) {
            std::cerr << "Failed to send data" << std::endl;
            closesocket(socket);
            closesocket(clientSocket); 
            closesocket(serverSocket); 
            WSACleanup();
            return 1;
        }
        
    }

    int numPrimes = 0; 

    // launch threads to listen for responses from subservers 
    std::vector<std::thread> listeners;

    if (server1Connect) {
        listeners.emplace_back(listenForResponse, server1Socket, std::ref(numPrimes));
    }

    if (server2Connect) {
        listeners.emplace_back(listenForResponse, server2Socket, std::ref(numPrimes)); 
    }

    // main server doing own work 
    int temp = launchThreads(indices[0].first, indices[0].second);
    std::cout << "main caluclated = " << temp << std::endl; 

    numPrimesMutex.lock(); 
    numPrimes += temp; 
    numPrimesMutex.unlock(); 

    // wait for listener threads 
    for (auto& listener : listeners) {
        listener.join();
    }

    // send number of primes to client  

    int totalPrimes = numPrimes; // TODO: add numbers returned from other servers

    int numPrimesMessage = htonl(totalPrimes);
    send(clientSocket, reinterpret_cast<char*>(&numPrimesMessage), sizeof(numPrimesMessage), 0);

    // Cleanup
    closesocket(clientSocket);
    closesocket(server1Socket);
    closesocket(server2Socket); 
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

void listenForResponse(SOCKET server, int& numPrimes) {
    // receive messages from sub servers 
    int num;
    int bytesReceived = recv(server, reinterpret_cast<char*>(&num), sizeof(num), 0);

    if (bytesReceived > 0) {
        // Convert from network byte order to host byte order
        num = ntohl(num);
        numPrimesMutex.lock(); 
        numPrimes += num; 
        numPrimesMutex.unlock(); 
    }
    else if (bytesReceived == 0) {
        std::cout << "Connection closed by server1...\n";
    }
    else {
        std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
    }
}

// returns number of primes from a list of numbers
int launchThreads(int start, int end) {
    // get primes 
    std::vector<int> primes;
    std::vector<std::thread> threads;

    // launch threads 
    for (int i = 0; i < N_THREADS; i++) {
        int division = (end - start + 1) / N_THREADS;
        int lowerBound = division * i + start;
        int upperBound = end + 1;

        if (i < N_THREADS - 1) {
            upperBound = division * (i + 1) + start;
        }
        threads.emplace_back(getPrimes, lowerBound, upperBound, std::ref(primes));
    }

    // wait for threads to finish 
    for (auto& thread : threads) {
        thread.join();
    }

    return primes.size(); 
}

bool checkPrime(const int& n) {
    if (n == 1) {
        return false; 
    }

    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

void getPrimes(int lowerBound, int upperBound, std::vector<int>& primes) {
    for (int i = lowerBound; i < upperBound; i++) {
        if (checkPrime(i)) {
            std::unique_lock<std::mutex> lock(myMutex);
            primes.push_back(i);
            lock.unlock();
        }
    }
}