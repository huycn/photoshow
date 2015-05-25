#pragma once
#include <vector>
#include <string>
#include <functional>

// return true if directory exists
bool ListFilesInDirectory(const std::wstring &dirPath, std::vector<std::wstring> &outNameList, std::function<bool(const std::wstring &)> filter);
