#pragma once

template <typename StringType>
bool EndsWith(const StringType &fullString, const StringType &ending)
{
	if (fullString.length() >= ending.length())
		return fullString.compare(fullString.length() - ending.length(), ending.length(), ending) == 0;
	return false;
}