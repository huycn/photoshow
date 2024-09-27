#include "Configuration.h"
#include <windows.h>

namespace {

	bool GetRegistryValue(HKEY key, LPCTSTR name, std::wstring& value)
	{
		DWORD dwtype = 0;
		DWORD dsize = 0;
		int sizeOfChar = sizeof(wchar_t);
		if (RegQueryValueEx(key, name, NULL, &dwtype, NULL, &dsize) == ERROR_SUCCESS) {
			if (dwtype == REG_SZ || dwtype == REG_EXPAND_SZ) {
				value.resize(dsize / sizeOfChar);
				if (RegQueryValueEx(key, name, NULL, NULL, (LPBYTE)value.data(), &dsize) == ERROR_SUCCESS) {
					// the result includes last null byte
					value.resize(value.size() - 1);
					return true;
				}
			}
		}
		return false;
	}

	bool GetRegistryValue(HKEY key, LPCTSTR name, int32_t& value)
	{
		DWORD dwtype = 0;
		DWORD dsize = sizeof(value);
		return RegQueryValueEx(key, name, NULL, &dwtype, (LPBYTE)&value, &dsize) == ERROR_SUCCESS && dwtype == REG_DWORD;
	}

	bool GetRegistryValue(HKEY key, LPCTSTR name, uint8_t& value)
	{
		int32_t val = 0;
		auto success = GetRegistryValue(key, name, val);
		if (success) {
			value = static_cast<uint8_t>(val);
		}
		return success;
	}

	bool GetRegistryValue(HKEY key, LPCTSTR name, bool& value)
	{
		int32_t intValue;
		if (GetRegistryValue(key, name, intValue)) {
			value = intValue != 0;
			return true;
		}
		return false;
	}

	bool SetRegistryValue(HKEY key, LPCTSTR name, const std::wstring& value)
	{
		return RegSetValueEx(key, name, 0, REG_SZ, (LPBYTE)value.c_str(), (value.size() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS;
	}

	bool SetRegistryValue(HKEY key, LPCTSTR name, int32_t value)
	{
		return RegSetValueEx(key, name, 0, REG_DWORD, (LPBYTE)&value, sizeof(int32_t)) == ERROR_SUCCESS;
	}

	bool SetRegistryValue(HKEY key, LPCTSTR name, bool value)
	{
		return SetRegistryValue(key, name, value ? 1 : 0);
	}

}

Configuration::Configuration()
:	interval(10),
	shuffle(true),
	transparency(0),
	clickThrough(false)
{}

Configuration Configuration::Load() {
	Configuration config;
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\SimpleSlideShow", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
		GetRegistryValue(key, L"Folders", config.folders);
		GetRegistryValue(key, L"Interval", config.interval);
		GetRegistryValue(key, L"Shuffle", config.shuffle);
		GetRegistryValue(key, L"Transparency", config.transparency);
		GetRegistryValue(key, L"ClickThrough", config.clickThrough);
		RegCloseKey(key);
	}
	return config;
}

void Configuration::Save() const {
	HKEY key;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\SimpleSlideShow", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
		SetRegistryValue(key, L"Folders", folders);
		SetRegistryValue(key, L"Interval", interval);
		SetRegistryValue(key, L"Shuffle", shuffle);
		SetRegistryValue(key, L"Transparency", transparency);
		SetRegistryValue(key, L"ClickThrough", clickThrough);
		RegCloseKey(key);
	}
}
