#include "worker/Worker.h"
#include "common/Consts.h"
#include <iostream>

/// Worker process that receives a list of URLs and reports the result
/// Example:
///    ./worker localhost 4242
/// The worker then contacts the leader process on "localhost" port "4242" for work
int main(int argc, char *argv[])
{
    bool local = false;

    if (argc < 3) {
	std::cerr << "Usage: " << argv[0] << " <host> <port> [-local]" << std::endl;
	return 1;
    }
    if (argc > 3) {
	auto param = std::string(argv[3]);
	if (param == "-local") {
	    local = true;
	}
    }
    auto *host = argv[1];
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
	opts.client->setContainer("cbdp-assignment-5");
    }

    Worker worker(host, port);
    while (true) {
	try {
	    worker.Init(opts, quit);
	    worker.Terminate();
	    break;
	} catch (const std::exception& e) {
	    //std::cerr << "Exception: " << e.what() << std::endl;
	}
    }
    return 0;
}
