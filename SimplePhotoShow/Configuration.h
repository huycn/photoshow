#pragma once
#include <string>

class Configuration
{
public:
	Configuration();

	std::wstring folders;
	int interval;	// in seconds
	bool shuffle;
	uint8_t transparency;
	bool clickThrough;

	static Configuration Load();
	void Save() const;
};

