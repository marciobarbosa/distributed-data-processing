#include "coordinator/Coordinator.h"
#include "common/CurlEasyPtr.h"
#include "common/Common.h"

#include <set>
#include <queue>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

std::unique_ptr<Storage> Coordinator::AllocStorage()
{
    if (opts.instance == Instance::AzureCloud) {
	return std::make_unique<CloudAzure>();
    }
    return std::make_unique<Disk>();
}

Coordinator::Coordinator(int port, std::string listurl) : port(port), listurl(listurl)
{
    result = 0;
    network = std::make_unique<Network>(port, this);
}

void Coordinator::Init(Options& opts, std::shared_ptr<std::atomic<bool>> quit)
{
    this->opts = opts;
    this->quit = quit;
    network->InitServer(quit);
    storage = AllocStorage();
    storage->Init(opts);
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
	pending_work.push_back(it_busy->second);
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
	network->SendServer(socket, REQUEST, socket, url);
    }
    while (!busy_workers.empty()) {
	busy_workers_cv.wait(lock);
	if (quit->load())
	    return 0;
    }
    while (pending_work.size() != 0) {
	std::vector<std::string> links = pending_work;
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

	    auto it = std::remove(pending_work.begin(), pending_work.end(), url);
	    pending_work.erase(it, pending_work.end());
	    network->SendServer(socket, REQUEST, socket, url);
	}
	while (!busy_workers.empty()) {
	    busy_workers_cv.wait(lock);
	    if (quit->load())
		return 0;
	}
    }
    return result;
}

void Coordinator::DistributeAggregation()
{
    std::unique_lock<std::mutex> lock(mtx);
    std::set<std::string> unique_files;
    std::vector<std::string> ranges;

    auto files = storage->GetFiles();
    for (const auto& file : files) {
	size_t pos = file.find('-');
	unique_files.insert(file.substr(0, pos));
    }

    size_t files_per_worker = unique_files.size() / idle_workers.size();
    ranges = GetRanges(unique_files, files_per_worker);
    for (auto range : ranges) {
	if (quit->load()) {
	    return;
	}
	while (idle_workers.size() == 0) {
	    idle_workers_cv.wait(lock);
	    if (quit->load())
		return;
	}
	int socket = idle_workers.back();
	idle_workers.pop_back();
	busy_workers[socket] = range;
	network->SendServer(socket, AGGREGATE, socket, range);
    }
    while (!busy_workers.empty()) {
	busy_workers_cv.wait(lock);
	if (quit->load())
	    return;
    }
    while (pending_work.size() != 0) {
	for (std::string range : pending_work) {
	    if (quit->load()) {
		return;
	    }
	    while (idle_workers.size() == 0) {
		idle_workers_cv.wait(lock);
		if (quit->load())
		    return;
	    }
	    int socket = idle_workers.back();
	    idle_workers.pop_back();
	    busy_workers[socket] = range;

	    auto it = std::remove(pending_work.begin(), pending_work.end(), range);
	    pending_work.erase(it, pending_work.end());
	    network->SendServer(socket, REQUEST, socket, range);
	}
	while (!busy_workers.empty()) {
	    busy_workers_cv.wait(lock);
	    if (quit->load())
		return;
	}
    }
    storage->RemoveFiles(files);
}

struct AggrEntry {
    int index;
    Blob blob_entry;
};

struct CompareAggrEntry {
    bool operator()(const AggrEntry& e1, const AggrEntry& e2) const {
        return e1.blob_entry.n_entries < e2.blob_entry.n_entries;
    }
};

std::vector<std::pair<std::string, int>> Coordinator::Aggregate(int top_size)
{
    std::string prefix = "aggr";
    std::vector<std::pair<std::string, int>> result;
    auto files = storage->GetFiles();

    std::vector<std::unique_ptr<Storage>> handles;
    for (const auto& file : files) {
	if (file.compare(0, prefix.length(), prefix) != 0)
	    continue;
	std::unique_ptr<Storage> handle = AllocStorage();
	handle->Init(opts);
	handle->Open(file, Mode::Read);
	handles.push_back(std::move(handle));
    }

    std::priority_queue<AggrEntry, std::vector<AggrEntry>, CompareAggrEntry> maxheap;
    for (int i = 0; i < static_cast<int>(handles.size()); i++) {
	AggrEntry entry = {};
	entry.index = i;
	handles[i]->Read(entry.blob_entry);
	maxheap.push(entry);
    }

    while (!maxheap.empty()) {
	AggrEntry entry = maxheap.top();
	maxheap.pop();

	result.emplace_back(entry.blob_entry.url, entry.blob_entry.n_entries);
	if (static_cast<int>(result.size()) == top_size)
	    break;
	if (!handles[entry.index]->Read(entry.blob_entry))
	    continue;
	maxheap.push(entry);
    }

    for (const auto& handle : handles) {
	handle->Close();
	handle->Destroy();
    }
    storage->RemoveFiles(files);
    return result;
}

void Coordinator::Terminate()
{
    quit->store(true);
    idle_workers_cv.notify_one();
    busy_workers_cv.notify_one();
    network->TerminateServer();
    storage->Destroy();
}
