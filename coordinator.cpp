#include "Coordinator.h"
#include <iostream>

/// Leader process that coordinates workers. Workers connect on the specified port
/// and the coordinator distributes the work of the CSV file list.
/// Example:
///    ./coordinator http://example.org/filelist.csv 4242
int main(int argc, char *argv[])
{
    if (argc != 3) {
	std::cerr << "Usage: " << argv[0] << " <URL to csv list> <listen port>" << std::endl;
	return 1;
    }
    auto listurl = std::string(argv[1]);
    auto port = std::stoi(argv[2]);
    auto quit = std::make_shared<std::atomic<bool>>(false);

    Coordinator coord(port, listurl);
    coord.Init(quit);

    auto result = coord.DistributeLoad();
    std::cout << result << std::endl;
    coord.Terminate();

    return 0;
}

