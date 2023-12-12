#include "Common.h"
#include "CurlEasyPtr.h"

#include <string>
#include <sstream>
#include <functional>
#include <sstream>
#include <iomanip>

using namespace std::literals;

const int HASH_LEN = 2;

std::string HashURL(const std::string& url)
{
    std::string result;
    std::stringstream ss;
    std::size_t hashvalue = std::hash<std::string>{}(url);

    ss << std::hex << std::setw(HASH_LEN) << std::setfill('0') << hashvalue;
    result = ss.str().substr(0, HASH_LEN);

    return result;
}

std::vector<std::string> GetRanges(const std::set<std::string> files, std::size_t delta)
{
    size_t n_files = files.size();
    std::vector<std::string> ranges;

    for (auto it = files.begin(); it != files.end(); it++) {
	std::string range = *it + "-";
	n_files--;

        for (std::size_t i = 1; i < delta && std::next(it) != files.end(); i++) {
	    it++;
	    n_files--;
        }
	if (n_files < delta) {
	    it = files.end();
	    it--;
	}
	range += *it;
	ranges.push_back(range);
    }
    return ranges;
}

static bool startsWith(const std::string& str, const std::string& prefix)
{
    return str.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> GetFileSubset(std::string begin_prefix, std::string end_prefix, std::vector<std::string> files)
{
    auto begin_it = files.end();
    auto end_it = files.end();
    std::vector<std::string> subset;

    for (auto it = files.begin(); it != files.end(); it++) {
	if (begin_it == files.end() && startsWith(*it, begin_prefix))
	    begin_it = it;
	if (startsWith(*it, end_prefix))
	    end_it = it;
    }
    end_it++;

    for (auto it = begin_it; it != end_it; it++) {
	if (startsWith(*it, "aggr"))
	    continue;
	subset.push_back(*it);
    }
    return subset;
}


static std::string_view getDomain(std::string_view url)
{
    auto pos = url.find("://"sv);

    if (pos != std::string::npos) {
	auto afterProtocol = std::string_view(url).substr(pos + 3);
	auto endDomain = afterProtocol.find('/');
	return afterProtocol.substr(0, endDomain);
    }
    return url;
}

size_t ProcessData(std::string url, std::unordered_map<std::string, int>& url_counts)
{
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
	    std::string domain(getDomain(column));
	    auto it = url_counts.find(domain);
	    if (it == url_counts.end()) {
		url_counts[domain] = 1;
		break;
	    }
	    url_counts[domain]++;
	    break;
	}
    }
    return 0;
}
