#include "Worker.h"
#include "Common.h"

#include <iostream>
#include <cstring>

Worker::Worker(const char *host, int port) : host(host), port(port)
{
    network = std::make_unique<Network>(host, port, this);
}

void Worker::Init(std::shared_ptr<std::atomic<bool>> quit)
{
    this->quit = quit;
    network->InitClient(quit);
}

void Worker::Request(std::string url)
{
    size_t result = ProcessData(url);
    network->SendClient(RESULT, static_cast<int>(result));
}

void Worker::Terminate()
{
    network->TerminateClient();
}
