#include "FileUtil.h"
#include <windows.h>
#include <strsafe.h>

bool ListFilesInDirectory(const std::wstring &dirPath, std::vector<std::wstring> &outNameList, std::function<bool(const std::wstring &)> filter)
{
	if (dirPath.length() > (MAX_PATH - 3))
		return false;

	TCHAR szDir[MAX_PATH];

	StringCchCopy(szDir, MAX_PATH, dirPath.c_str());
	StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(szDir, &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			std::wstring fileName = ffd.cFileName;
			if (filter == nullptr || filter(fileName))
				outNameList.push_back(fileName);
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	return true;
}
