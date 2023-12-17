#ifndef COMMON_H
#define COMMON_H

#include <set>
#include <vector>
#include <iostream>
#include <unordered_map>

std::string HashURL(const std::string& url);
size_t ProcessData(std::string url, std::unordered_map<std::string, int>& url_counts);
std::vector<std::string> GetRanges(const std::set<std::string> files, std::size_t delta);
std::vector<std::string> GetFileSubset(std::string begin_prefix, std::string end_prefix, std::vector<std::string> files);

#endif
