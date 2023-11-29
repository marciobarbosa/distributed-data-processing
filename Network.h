#ifndef NETWORK_H
#define NETWORK_H

#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

// Constants
const int BUF_SIZE = 1024;
const int HEARTBEATS_SECS = 15;

// Structs
struct Packet {
    int opcode;
    int value;
    char message[BUF_SIZE];
};
const int PKTSIZE = sizeof(Packet);

// Opcodes
const int RESULT = 0;
const int HEARTBEAT = 1;
const int REQUEST = 2;

class Client {
public:
    virtual void Request(std::string) = 0;
};

class Server {
public:
    virtual void NewClient(int) = 0;
    virtual void Request(int, int) = 0;
    virtual void CrashedClient(int) = 0;
};

class Network {
public:
    Network();
    Network(int port, Server *server);
    Network(const char *host, int port, Client *client);
    void InitServer(std::shared_ptr<std::atomic<bool>> quit);
    void InitClient(std::shared_ptr<std::atomic<bool>> quit);
    bool SendServer(int socket, int opcode, int value, std::string message);
    bool SendClient(int opcode, int value);
    void TerminateServer();
    void TerminateClient();
private:
    int sock;
    int port;
    std::mutex mtx;
    std::thread rcv_t;
    std::shared_ptr<std::atomic<bool>> quit;

    // Server specific
    std::thread conn_t;
    std::thread heartbeat_t;
    std::vector<int> clients;
    std::vector<int> headbeats;

    // Client specific
    const char *host;

    void ResolveServer(Packet& pkt, int socket);
    void ResolveClient(Packet& pkt);
    void AcceptConnections();
    void ReceiverServer();
    void ReceiverClient();
    void HeartBeatsBroadcast();
    void HeartBeatsRcv(int socket);

    void Deserialize(const char *buffer, size_t bufsize, Packet& pkt);
    void Serialize(const Packet& pkt, char *buffer, size_t bufsize);

    // Callbacks
    Server *server;
    Client *client;
};

#endif
