#ifndef DISK_H

#include <iostream>
#include <fstream>
#include <string>
#include "Storage.h"

class Disk : public Storage {
public:
    void Init();
    void Destroy();
    void Open(const std::string& filename) override;
    bool Read(Blob& data) override;
    void Write(const Blob& data) override;
    void RemoveFiles(std::vector<std::string> files);
    std::vector<std::string> GetFiles() override;
    void Close() override;
private:
    std::string filename;
    std::fstream filestream;
    std::string basedir;
};

#endif
