#include "Worker.h"
#include <iostream>

/// Worker process that receives a list of URLs and reports the result
/// Example:
///    ./worker localhost 4242
/// The worker then contacts the leader process on "localhost" port "4242" for work
int main(int argc, char *argv[])
{
    if (argc != 3) {
	std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
	return 1;
    }
    auto *host = argv[1];
    auto port = std::stoi(argv[2]);
    auto quit = std::make_shared<std::atomic<bool>>(false);

    Worker worker(host, port);
    while (true) {
	try {
	    worker.Init(quit);
	    worker.Terminate();
	    break;
	} catch (const std::exception& e) {
	    //std::cerr << "Exception: " << e.what() << std::endl;
	}
    }
    return 0;
}
