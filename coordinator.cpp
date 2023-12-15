#include "AzureBlobClient.h"
#include "Coordinator.h"
#include "Consts.h"
#include <iostream>

/// Leader process that coordinates workers. Workers connect on the specified port
/// and the coordinator distributes the work of the CSV file list.
/// Example:
///    ./coordinator http://example.org/filelist.csv 4242
int main(int argc, char *argv[])
{
    bool local = false;

    if (argc < 3) {
	std::cerr << "Usage: " << argv[0] << " <URL to csv list> <listen port> [-local]" << std::endl;
	return 1;
    }
    if (argc > 3) {
	auto param = std::string(argv[3]);
	if (param == "-local") {
	    local = true;
	}
    }
    auto listurl = std::string(argv[1]);
    auto port = std::stoi(argv[2]);
    auto quit = std::make_shared<std::atomic<bool>>(false);

    Options opts;
    if (local) {
	opts.instance = Instance::Disk;
	opts.path = "/tmp/output/";
    } else {
	opts.instance = Instance::AzureCloud;
	opts.path = "/tmp/output/";
	opts.client = std::make_shared<AzureBlobClient>(accountName, accountToken);
	opts.client->createContainer("cbdp-assignment-5");
    }

    Coordinator coord(port, listurl);
    coord.Init(opts, quit);

    coord.DistributeLoad();
    coord.DistributeAggregation();

    auto result = coord.Aggregate(25);
    for (const auto& entry : result) {
	std::cout << entry.first << " " << entry.second << "\n";
    }

    coord.Terminate();
    if (!local) {
	opts.client->deleteContainer();
    }
    return 0;
}
