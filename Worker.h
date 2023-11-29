#ifndef WORKER_H
#define WORKER_H

#include "Network.h"

#include <memory>
#include <atomic>

class Worker : public Client {
public:
    Worker(const char *host, int port);
    void Init(std::shared_ptr<std::atomic<bool>> quit);
    void Terminate();

    // Callbacks called from the Network layer
    void Request(std::string url);
private:
    const char *host;
    int port;
    std::unique_ptr<Network> network;
    std::shared_ptr<std::atomic<bool>> quit;
};

#endif
