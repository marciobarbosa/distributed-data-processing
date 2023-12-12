#include "Worker.h"
#include "Common.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <unordered_map>

Worker::Worker(const char *host, int port) : host(host), port(port)
{
    n_partitions = 0;
    network = std::make_unique<Network>(host, port, this);
    storage = std::make_unique<Disk>();
}

void Worker::Init(std::shared_ptr<std::atomic<bool>> quit)
{
    this->quit = quit;
    storage->Init();
    network->InitClient(quit);
}

void Worker::Request(std::string url, int worker_id)
{
    std::unordered_map<std::string, int> url_counts;
    size_t result = ProcessData(url, url_counts);
    auto unique = std::to_string(worker_id);

    for (const auto& url_count : url_counts) {
	Blob blob = {};
	auto key = url_count.first;
	auto filename = HashURL(key) + "-" + unique;

	blob.n_entries = url_count.second;
	std::copy(key.begin(), key.end(), blob.url);

	storage->Open(filename);
	storage->Write(blob);
	storage->Close();

	n_partitions++;
    }
    network->SendClient(RESULT, static_cast<int>(result));
}

void Worker::Aggregate(std::string range, int worker_id)
{
    size_t result = 0;
    size_t pos = range.find('-');
    std::string begin = range.substr(0, pos);
    std::string end = range.substr(pos + 1);
    std::unordered_map<std::string, int> url_counts;

    auto files = storage->GetFiles();
    std::sort(files.begin(), files.end());
    auto filelst = GetFileSubset(begin, end, files);

    for (auto file : filelst) {
	Blob blob = {};
	storage->Open(file);

	while (storage->Read(blob)) {
	    auto it = url_counts.find(blob.url);
	    if (it == url_counts.end()) {
		url_counts[blob.url] = blob.n_entries;
		continue;
	    }
	    url_counts[blob.url] += blob.n_entries;
	}
	storage->Close();
    }
    std::vector<std::pair<std::string, int>> sorted_urls;
    for (const auto& entry : url_counts) {
	sorted_urls.push_back(entry);
    }
    std::sort(sorted_urls.begin(), sorted_urls.end(), [](const auto& a, const auto& b) {
	return a.second > b.second;
    });

    auto unique = std::to_string(worker_id) + "-" + std::to_string(n_partitions);
    auto filename = std::string("aggr") + "_" + unique;
    storage->Open(filename);

    for (const auto& pair : sorted_urls) {
	Blob blob = {};
	auto url = pair.first;

	blob.n_entries = pair.second;
	std::copy(url.begin(), url.end(), blob.url);
	storage->Write(blob);

    }
    storage->Close();
    n_partitions++;

    network->SendClient(RESULT, static_cast<int>(result));
}

void Worker::Terminate()
{
    network->TerminateClient();
    storage->Destroy();
}
