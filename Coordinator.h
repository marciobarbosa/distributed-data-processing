#ifndef COORDINATOR_H
#define COORDINATOR_H

#include "Network.h"

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <condition_variable>

class Coordinator : public Server {
public:
    Coordinator(int port, std::string listurl);
    void Init(std::shared_ptr<std::atomic<bool>> quit);
    size_t DistributeLoad();
    void Terminate();

    // Callbacks called from the Network layer
    void NewClient(int socket);
    void Request(int socket, int result);
    void CrashedClient(int socket);
private:
    int port;
    size_t result;
    std::mutex mtx;
    std::string listurl;
    std::unique_ptr<Network> network;
    std::shared_ptr<std::atomic<bool>> quit;

    std::vector<int> idle_workers;
    std::condition_variable idle_workers_cv;
    std::unordered_map<int, std::string> busy_workers;
    std::condition_variable busy_workers_cv;
    std::vector<std::string> pending_links;
};

#endif
