#include "storage/Disk.h"
#include <cstring>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

void Disk::Init(Options& opts)
{
    basedir = opts.path;
    if (!fs::is_directory(basedir))
	fs::create_directory(basedir);
}

void Disk::Destroy() {}

void Disk::Open(const std::string& filename, Mode mode)
{
    (void)mode;
    const std::string fullpath = basedir + filename;

    this->filename = filename;
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

bool Disk::Read(Blob& data)
{
    if (!filestream.is_open()) {
	throw std::runtime_error("File not open for reading");
    }
    if (filestream.read(reinterpret_cast<char*>(&data), sizeof(Blob))) {
	return true;
    }
    return false;
}

void Disk::Write(const Blob& data)
{
    if (!filestream.is_open()) {
	throw std::runtime_error("File not open for writing");
    }
    filestream.write(reinterpret_cast<const char*>(&data), sizeof(Blob));
}

std::vector<std::string> Disk::GetFiles()
{
    std::vector<std::string> files;

    for (const auto& entry : fs::directory_iterator(basedir)) {
	if (!entry.is_regular_file())
	    continue;
	files.push_back(entry.path().filename().string());
    }
    return files;
}

void Disk::RemoveFiles(std::vector<std::string> files)
{
    for (auto file : files) {
	auto fullpath = basedir + file;
	fs::remove(fullpath);
    }
}

void Disk::Close()
{
    if (filestream.is_open()) {
	filestream.close();
    }
}
