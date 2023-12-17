#include "storage/CloudAzure.h"
#include <cstring>
#include <stdexcept>
#include <filesystem>

#include "storage/AzureBlobClient.h"
#include "common/Consts.h"

namespace fs = std::filesystem;

void CloudAzure::Init(Options& opts)
{
    basedir = opts.path;
    client = opts.client;
    if (!fs::is_directory(basedir))
	fs::create_directory(basedir);
}

void CloudAzure::Destroy() {}

void CloudAzure::Open(const std::string& filename, Mode mode)
{
    const std::string fullpath = basedir + filename;

    this->mode = mode;
    this->filename = filename;

    if (mode == Mode::Read) {
	std::ofstream file(fullpath, std::ofstream::out | std::ofstream::trunc);
	if (!file.is_open()) {
	    throw std::runtime_error("Error opening file");
	}
	client->downloadStream(filename, file);
	file.close();
    }

    filestream.open(fullpath, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);

    if (!filestream.is_open()) {
	filestream.clear();
	filestream.open(fullpath, std::ios::out | std::ios::binary);
	filestream.close();
	filestream.open(fullpath, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    }
    if (!filestream.is_open()) {
	throw std::runtime_error("Error opening file");
    }
}

bool CloudAzure::Read(Blob& data)
{
    if (!filestream.is_open()) {
	throw std::runtime_error("File not open for reading");
    }
    if (filestream.read(reinterpret_cast<char*>(&data), sizeof(Blob))) {
	return true;
    }
    return false;
}

void CloudAzure::Write(const Blob& data)
{
    if (!filestream.is_open()) {
	throw std::runtime_error("File not open for writing");
    }
    filestream.write(reinterpret_cast<const char*>(&data), sizeof(Blob));
}

std::vector<std::string> CloudAzure::GetFiles()
{
    return client->listBlobs();
}

void CloudAzure::RemoveFiles(std::vector<std::string> files)
{
    for (auto file : files) {
	auto fullpath = basedir + file;
	fs::remove(fullpath);
    }
}

void CloudAzure::Close()
{
    if (filestream.is_open()) {
	filestream.close();
    }
    if (mode == Mode::Write) {
	std::ifstream file(basedir + filename);
	if (!file.is_open()) {
	    throw std::runtime_error("Could not open file");
	}
	client->uploadStream(filename, file);
	file.close();
    }
}
