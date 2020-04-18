// gitdisk.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "MsgQueue.h"

#include <iostream>
#include <vector>
#include <functional>
#include <filesystem>
#include <fstream>
#include <regex>
#include <map>
#include "Git.h"
#include <thread>

#include <Windows.h>
#include "Systray.h"


wchar_t winname[] = L"__gitdisk__";
wchar_t title[] = L"Gitdisk";
char config[] = "config.ini";
UINT WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
Systray* systray;
Git git;

MsgQueue GitDispatcher;


std::map<std::string, std::string> configs;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		systray = new Systray(hwnd, L"Gitdisk");
		systray->addMenu(L"config", MF_STRING, [=]() {
			//systray->setFlags(0, MF_CHECKED | MF_STRING, L"config");
			system("notepad config.ini");
		});
		systray->addMenu(L"", MF_SEPARATOR,{});
		systray->addMenu(L"exit", MF_STRING, [=]() {
			SendMessage(hwnd, WM_CLOSE, 0, 0);
		});
		break;
	case WM_USER:
		systray->process(lParam);
		break;
	case WM_DESTROY:
		delete systray;
		PostQuitMessage(0);
		break;
	default:
		if (message == WM_TASKBARCREATED)
			SendMessage(hwnd, WM_CREATE, wParam, lParam);
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}


void initWindow()
{
	HWND hwnd;


	HWND handle = FindWindowW(NULL, winname);
	if (handle != NULL)
	{
		MessageBoxW(NULL, L"Gitdisk is already running", title, MB_ICONERROR);
		return;
	}

	WNDCLASSW wndclass;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = NULL;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = winname;

	if (!RegisterClassW(&wndclass))
	{
		MessageBoxW(NULL, L"This program requires Windows NT!", winname, MB_ICONERROR);
		return;
	}

	hwnd = CreateWindowExW(WS_EX_TOOLWINDOW,
		winname, winname,
		WS_POPUP,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL, NULL, NULL, NULL);

	ShowWindow(hwnd, 0);
	UpdateWindow(hwnd);

}

void refreshConfig()
{
	if (!std::filesystem::exists(config))
	{
		std::fstream f(config, std::ios::out);
		f << "repository=" << std::endl;
		f << "localpath=" << std::endl;
	}

	std::fstream f(config, std::ios::in);
	std::vector<std::string> lines;
	std::string line;
	while (f >> line)
		lines.emplace_back(line);

	std::regex p("^([^#]\\S+)=(\\S+)$");
	std::map<std::string, std::string> cfgs;
	for (auto& l : lines)
	{
		std::smatch ret;
		if (!std::regex_search(l, ret, p))
			continue;

		cfgs[ret[1]] = ret[2];
	}

	auto end = cfgs.end();
	auto exec = [&](const char* key, auto f) {
		auto ret = cfgs.find(key);
		if (end == ret)
			return;
		if (configs[key] == ret->second)
			return;

		f(key, ret->second);
	};

	auto record = [&](const char* key) {
		exec(key, [&](auto key, auto value) {
			std::cout << key << ": " << value << std::endl;
			configs[key] = value;
		});
	};

	std::cout << "load config" <<std::endl;
	record("localpath");
	record("username");
	record("password");
	record("repository");


	GitDispatcher.addMsg([
		path = configs["localpath"], 
		repo = configs["repository"], 
		usr = configs["username"],
		pwd = configs["password"]](){
		git.init(path, repo, usr, pwd);
	});
}


int main()
{
	//auto handle = FindFirstChangeNotification(L"temp",true, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME);

	//do
	//	WaitForSingleObject(handle,-1);

	//while(::FindNextChangeNotification(handle));

	//git_libgit2_init();

	//git_repository* repo;

	//git_clone(&repo,"https://github.com/nustxujun/nautiloidea.git","temp", nullptr);

	//git_libgit2_shutdown();
	initWindow();
	refreshConfig();

	std::thread gitthread([](){
		GitDispatcher.run();
	});

	bool watching = true;
	std::thread monitorthread([&](){
		//auto handle = FindFirstChangeNotificationW(L"temp",false, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME);
		char buffer[1024] = {0};
		std::wregex re(L"^\\.git.*");
		FILE_NOTIFY_INFORMATION* pNotification = (FILE_NOTIFY_INFORMATION*)buffer;
		HANDLE file = CreateFileW(L"temp",
			GENERIC_READ | GENERIC_WRITE | FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			std::cout << "cannot create file: " <<  GetLastError() << std::endl;
			return;
		}
		do
		{	
			//HANDLE handles[]= {breaker, handle};
			//WaitForMultipleObjects(2, handles,FALSE, -1);
			//auto wo = WaitForSingleObject(handle, -1);
			DWORD receive = 0;
			::ReadDirectoryChangesW(file,
				buffer, sizeof(buffer), TRUE,
				FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
				&receive, NULL, NULL);
			if (receive != 0 && !std::regex_match( std::wstring(pNotification->FileName) ,re))
				GitDispatcher.addMsg([&]() {
					git.sync();
				});
		}while(watching);

		::CloseHandle(file);

	});
	auto ftime = std::filesystem::last_write_time(config);
	MSG msg = {};
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			auto now = std::filesystem::last_write_time(config);
			if (now != ftime)
				refreshConfig();
		}
	}

	watching = false;
	monitorthread.join();

	GitDispatcher.close();
	gitthread.join();


	return 0;
}

