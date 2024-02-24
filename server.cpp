#include <iostream>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <random>

#define N_THREADS 32

int PORT = 6250;
int SERVER1_PORT = 6251;
int SERVER2_PORT = 6252;

bool checkPrime(const int& n);
void getPrimes(int lowerBound, int upperBound, std::vector<int>& numList, std::vector<int>& primes);
int launchThreads(std::vector<int> numList);

std::mutex myMutex;

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

    // send request to client to get n 
    const char* sendData = "Please enter n: ";
    send(clientSocket, sendData, strlen(sendData), 0);

    // Receiving an n from client
    int n;
    int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&n), sizeof(n), 0);

    if (bytesReceived > 0) {
        // Convert from network byte order to host byte order
        n = ntohl(n);
        std::cout << "Received integer: " << n << std::endl;
    }
    else if (bytesReceived == 0) {
        std::cout << "Connection closed by client...\n";
    }
    else {
        std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
    }

    // generate list 
    std::vector<int> mainNumList;

    for (int i = 2; i <= n; i++) {
        mainNumList.emplace_back(i); 
    }

    // shuffle list 
    std::random_device rd;
    std::mt19937 rng(rd());

    std::shuffle(mainNumList.begin(), mainNumList.end(), rng);

    // slice list 
    std::vector<std::vector<int>> slices; 

    for (int i = 0; i < serverConnections; i++) {
        int division = mainNumList.size() / serverConnections; 
        int start = division * i; 
        int end = mainNumList.size();

        if (i < serverConnections - 1) {
            end = division * (i + 1); 
        }

        std::vector<int> slice(mainNumList.begin() + start, mainNumList.begin() + end);

        slices.emplace_back(slice); 
    }

    // sending messages to sub servers  
    for (int i = 1; i < serverConnections; i++) { // start at 1 to exclude slice of main server 
        std::vector<char> byteStream(slices[i].begin(), slices[i].end()); 

        int bytesSent = (i == 1 && server1Connect) ?
            send(server1Socket, byteStream.data(), byteStream.size(), 0) : 
            send(server2Socket, byteStream.data(), byteStream.size(), 0);

        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "Failed to send data" << std::endl;
            if (server1Connect) closesocket(server1Socket);
            if (server2Connect) closesocket(server2Socket); 
            closesocket(clientSocket); 
            closesocket(serverSocket); 
            WSACleanup();
            return 1;
        }
    }

    // main server doing own work 
    int numPrimes = launchThreads(slices[0]); 

    // receive messages from sub servers 
    if (server1Connect) {
        int num; 
        bytesReceived = recv(server1Socket, reinterpret_cast<char*>(&num), sizeof(num), 0);

        if (bytesReceived > 0) {
            // Convert from network byte order to host byte order
            num = ntohl(num);
            numPrimes += num; 
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server1...\n";
        }
        else {
            std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
        }
    }

    if (server2Connect) {
        int num;
        bytesReceived = recv(server2Socket, reinterpret_cast<char*>(&num), sizeof(num), 0);

        if (bytesReceived > 0) {
            // Convert from network byte order to host byte order
            num = ntohl(num);
            numPrimes += num;
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server2...\n";
        }
        else {
            std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
        }
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

// returns number of primes from a list of numbers
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