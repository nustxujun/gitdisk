#include "Systray.h"

Systray::Systray(HWND hwnd, const std::wstring& name)
{
    mWindow = hwnd;
    mNID.cbSize = sizeof(mNID);
    mNID.hWnd = hwnd;
    mNID.uID = 0;
    mNID.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    mNID.uCallbackMessage = WM_USER;
    mNID.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    memcpy(mNID.szTip, name.data(), (name.length() + 1) * sizeof(wchar_t));
    Shell_NotifyIconW(NIM_ADD, &mNID);
    mHandle = CreatePopupMenu();
    mMenus.emplace_back([](){});
}

Systray::~Systray()
{
    Shell_NotifyIconW(NIM_DELETE, &mNID);
}

size_t Systray::addMenu(const std::wstring& name, UINT flags, std::function<void()>&& f)
{
    auto index = mMenus.size();
    AppendMenuW(mHandle, flags, index, name.c_str());
    mMenus.push_back(f);
    return index;
}

void Systray::setFlags(UINT index, UINT flags, const wchar_t* content)
{
    ModifyMenuW(mHandle,index,flags | MF_BYCOMMAND, index, content);
}

void Systray::process(LPARAM msg)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        break;
    case WM_RBUTTONDOWN:
    {
        POINT pt;
        GetCursorPos(&pt);
        ::SetForegroundWindow(mWindow);//解决在菜单外单击左键菜单不消失的问题
        //EnableMenuItem(hmenu, 0, MF_GRAYED);//让菜单中的某一项变灰
        //EnableMenuItem(mHandle, 1, MF_CHECKED | MF_STRING);

        auto index = TrackPopupMenu(mHandle, TPM_RETURNCMD, pt.x, pt.y, NULL, mWindow, NULL);
        mMenus[index]();
    }
    break;
    default:
        break;
    }
}
