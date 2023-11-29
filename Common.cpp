#include "Common.h"
#include "CurlEasyPtr.h"

#include <sstream>
#include <string>

using namespace std::literals;

size_t ProcessData(std::string url)
{
    size_t result = 0;
    auto curl = CurlEasyPtr::easyInit();

    curl.setUrl(url);
    auto csvData = curl.performToStringStream();

    for (std::string row; std::getline(csvData, row, '\n');) {
	auto rowStream = std::stringstream(std::move(row));

	// Check the URL in the second column
	unsigned columnIndex = 0;
	for (std::string column; std::getline(rowStream, column, '\t'); ++columnIndex) {
	    // column 0 is id, 1 is URL
	    if (columnIndex != 1)
		continue;
	    // Check if URL is "google.ru"
	    auto pos = column.find("://"sv);
	    if (pos != std::string::npos) {
		auto afterProtocol = std::string_view(column).substr(pos + 3);
		if (afterProtocol.starts_with("google.ru/"))
		    ++result;
	    }
	    break;
	}
    }
    return result;
}
