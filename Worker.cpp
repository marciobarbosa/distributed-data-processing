#include "Worker.h"
#include "Common.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <unordered_map>

std::unique_ptr<Storage> Worker::AllocStorage()
{
    if (opts.instance == Instance::AzureCloud) {
	return std::make_unique<CloudAzure>();
    }
    return std::make_unique<Disk>();
}

Worker::Worker(const char *host, int port) : host(host), port(port)
{
    n_partitions = 0;
    network = std::make_unique<Network>(host, port, this);
}

void Worker::Init(Options& opts,std::shared_ptr<std::atomic<bool>> quit)
{
    this->opts = opts;
    this->quit = quit;
    storage = AllocStorage();
    storage->Init(opts);
    network->InitClient(quit);
}

void Worker::Request(std::string url, int worker_id)
{
    std::unordered_map<std::string, int> url_counts;
    size_t result = ProcessData(url, url_counts);
    auto unique = std::to_string(worker_id);
    std::unordered_map<std::string, std::unique_ptr<Storage>> filetable;
    std::unique_ptr<Storage> storageptr;

    for (const auto& url_count : url_counts) {
	Blob blob = {};
	auto key = url_count.first;
	auto filename = HashURL(key) + "-" + unique;

	blob.n_entries = url_count.second;
	std::copy(key.begin(), key.end(), blob.url);

	auto it = filetable.find(filename);
	if (it == filetable.end()) {
	    std::unique_ptr<Storage> file = AllocStorage();
	    file->Init(opts);
	    file->Open(filename, Mode::Write);
	    filetable[filename] = std::move(file);
	}
	filetable[filename]->Write(blob);
	n_partitions++;
    }
    for (const auto& entry : filetable) {
	entry.second->Close();
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
	storage->Open(file, Mode::Read);

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
    storage->Open(filename, Mode::Write);

    size_t n_entries = 0;
    size_t max_entries = 50000;

    for (const auto& pair : sorted_urls) {
	Blob blob = {};
	auto url = pair.first;

	blob.n_entries = pair.second;
	std::copy(url.begin(), url.end(), blob.url);
	storage->Write(blob);

	n_entries++;
	if (n_entries == max_entries) {
	    storage->Close();
	    n_partitions++;
	    unique = std::to_string(worker_id) + "-" + std::to_string(n_partitions);
	    filename = std::string("aggr") + "_" + unique;
	    storage->Open(filename, Mode::Write);
	    n_entries = 0;
	}
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
