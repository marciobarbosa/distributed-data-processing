#ifndef WORKER_H
#define WORKER_H

#include "network/Network.h"
#include "storage/Storage.h"
#include "storage/Disk.h"
#include "storage/CloudAzure.h"

#include <memory>
#include <atomic>

class Worker : public Client {
public:
    Worker(const char *host, int port);
    void Init(Options& opts, std::shared_ptr<std::atomic<bool>> quit);
    void Terminate();

    // Callbacks called from the Network layer
    void Request(std::string url, int worker_id);
    void Aggregate(std::string url, int worker_id);
private:
    const char *host;
    int port;
    Options opts;
    int n_partitions;
    std::unique_ptr<Network> network;
    std::unique_ptr<Storage> storage;
    std::shared_ptr<std::atomic<bool>> quit;

    std::unique_ptr<Storage> AllocStorage();
};

#endif
