#ifndef WORKER_H
#define WORKER_H

#include "Network.h"
#include "Storage.h"
#include "Disk.h"

#include <memory>
#include <atomic>

class Worker : public Client {
public:
    Worker(const char *host, int port);
    void Init(std::shared_ptr<std::atomic<bool>> quit);
    void Terminate();

    // Callbacks called from the Network layer
    void Request(std::string url, int worker_id);
    void Aggregate(std::string url, int worker_id);
private:
    const char *host;
    int port;
    int n_partitions;
    std::unique_ptr<Network> network;
    std::unique_ptr<Storage> storage;
    std::shared_ptr<std::atomic<bool>> quit;
};

#endif
