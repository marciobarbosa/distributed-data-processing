#ifndef STORAGE_H
#define STORAGE_H

#include <vector>
#include "storage/AzureBlobClient.h"

const int URL_MAX = 2048;

enum class Mode {
    Read,
    Write
};

enum class Instance {
    Disk,
    AzureCloud
};

struct Options {
    Instance instance;
    std::string path;
    std::shared_ptr<AzureBlobClient> client;
};

struct Blob {
    int n_entries;
    char url[URL_MAX];
};

class Storage {
public:
    virtual void Init(Options& opts) = 0;
    virtual void Destroy() = 0;
    virtual void Open(const std::string& filename, Mode mode) = 0;
    virtual bool Read(Blob& data) = 0;
    virtual void Write(const Blob& data) = 0;
    virtual void Close() = 0;
    virtual void RemoveFiles(std::vector<std::string> files) = 0;
    virtual std::vector<std::string> GetFiles() = 0;
    virtual ~Storage() = default;
};

#endif
