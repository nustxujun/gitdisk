#pragma once

#include <windows.h>
#include <vector>
#include <functional>
#include <string>
class Systray
{
public:
	Systray(HWND hwnd, const std::wstring& name);
	~Systray();

	size_t addMenu(const std::wstring& name, UINT flags, std::function<void()>&& f);
	void setFlags(UINT index, UINT flags, const wchar_t* content);
	void process(LPARAM msg);
private:
	HWND mWindow;
	HMENU mHandle;
	NOTIFYICONDATAW mNID;
	std::vector<std::function<void()>> mMenus;

};