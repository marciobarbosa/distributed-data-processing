#include "Coordinator.h"
#include "CurlEasyPtr.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

Coordinator::Coordinator(int port, std::string listurl) : port(port), listurl(listurl)
{
    result = 0;
    network = std::make_unique<Network>(port, this);
}

void Coordinator::Init(std::shared_ptr<std::atomic<bool>> quit)
{
    this->quit = quit;
    network->InitServer(quit);
}

void Coordinator::NewClient(int socket)
{
    std::unique_lock<std::mutex> lock(mtx);
    idle_workers.push_back(socket);
    idle_workers_cv.notify_one();
}

void Coordinator::Request(int socket, int result)
{
    std::unique_lock<std::mutex> lock(mtx);
    this->result += result;

    auto it = busy_workers.find(socket);
    if (it != busy_workers.end()) {
	busy_workers.erase(it);
	busy_workers_cv.notify_one();
    }
    idle_workers.insert(idle_workers.begin(), socket);
    idle_workers_cv.notify_one();
}

void Coordinator::CrashedClient(int socket)
{
    std::unique_lock<std::mutex> lock(mtx);

    auto it_busy = busy_workers.find(socket);
    if (it_busy != busy_workers.end()) {
	pending_links.push_back(it_busy->second);
	busy_workers.erase(it_busy);
	busy_workers_cv.notify_one();
	return;
    }

    auto it_idle = std::remove(idle_workers.begin(), idle_workers.end(), socket);
    if (it_idle != idle_workers.end()) {
	idle_workers.erase(it_idle, idle_workers.end());
    }
}

size_t Coordinator::DistributeLoad()
{
    std::unique_lock<std::mutex> lock(mtx);
    CurlGlobalSetup();

    auto curl = CurlEasyPtr::easyInit();
    curl.setUrl(listurl);
    auto filelst = curl.performToStringStream();

    for (std::string url; std::getline(filelst, url, '\n');) {
	if (quit->load()) {
	    return 0;
	}
	while (idle_workers.size() == 0) {
	    idle_workers_cv.wait(lock);
	    if (quit->load())
		return 0;
	}
	int socket = idle_workers.back();
	idle_workers.pop_back();
	busy_workers[socket] = url;
	network->SendServer(socket, REQUEST, 0, url);
    }
    while (!busy_workers.empty()) {
	busy_workers_cv.wait(lock);
	if (quit->load())
	    return 0;
    }
    while (pending_links.size() != 0) {
	std::vector<std::string> links = pending_links;
	for (std::string url : links) {
	    if (quit->load()) {
		return 0;
	    }
	    while (idle_workers.size() == 0) {
		idle_workers_cv.wait(lock);
		if (quit->load())
		    return 0;
	    }
	    int socket = idle_workers.back();
	    idle_workers.pop_back();
	    busy_workers[socket] = url;

	    auto it = std::remove(pending_links.begin(), pending_links.end(), url);
	    pending_links.erase(it, pending_links.end());
	    network->SendServer(socket, REQUEST, 0, url);
	}
	while (!busy_workers.empty()) {
	    busy_workers_cv.wait(lock);
	    if (quit->load())
		return 0;
	}
    }
    return result;
}

void Coordinator::Terminate()
{
    quit->store(true);
    idle_workers_cv.notify_one();
    busy_workers_cv.notify_one();
    network->TerminateServer();
}
