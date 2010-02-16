// Copyright © 2010 Martin Stone.
// 
// This file is part of TouchCursor.
// 
// TouchCursor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// TouchCursor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with TouchCursor.  If not, see <http://www.gnu.org/licenses/>.


#define _WIN32_IE 0x0500 // for Shell_NotifyIcon version

#include "launch.h"
#include <windows.h>
#include "resource.h"
#include <ctime>


__declspec(dllimport) void InstallHook();
__declspec(dllimport) void RemoveHook();
__declspec(dllimport) void LoadOptions();
__declspec(dllimport) bool IsEnabled();
__declspec(dllimport) bool ShouldShowInNotificationArea();
__declspec(dllimport) void ReHook();
__declspec(dllimport) void CheckForUpdate();

namespace {

    static HINSTANCE Hinst;

    const int taskbarNotificationMsg = WM_USER;
    const int loadOptionsMsg = WM_USER + 1;
    const wchar_t* const windowTitle = L"TouchCursor";


    DWORD showMenu(HWND hwnd) { 
        POINT pt;
        GetCursorPos(&pt);
        HMENU hmenu = LoadMenu(Hinst, MAKEINTRESOURCE(IDR_MENU1));
        // TrackPopupMenu cannot display the menu bar so get 
        // a handle to the first shortcut menu. 
        HMENU hmenuTrackPopup = GetSubMenu(hmenu, 0);

        // Display the shortcut menu. 
        DWORD command = TrackPopupMenu(hmenuTrackPopup, 
                TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, 
                pt.x, pt.y, 0, hwnd, NULL); 
     
        DestroyMenu(hmenu);
        return command;
    }

    void runConfigure() {
        LaunchProgRelative(L"tcconfig.exe");
    }

    void showHelp() {
        LaunchLocalHtml(L"docs\\help.html");
    }

    void refreshIconState(HWND hwnd, bool justDelete=false) {
        // delete, maybe re-add after

        NOTIFYICONDATA nid;
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 0;

        Shell_NotifyIcon(NIM_DELETE, &nid);

        if (!justDelete && ShouldShowInNotificationArea()) {
            // add the item & set version
            nid.uFlags = NIF_MESSAGE;
            nid.uCallbackMessage = taskbarNotificationMsg;
            nid.uVersion = NOTIFYICON_VERSION;
            Shell_NotifyIcon(NIM_ADD, &nid);
            Shell_NotifyIcon(NIM_SETVERSION, &nid);

            // set the icon
            nid.uFlags = NIF_ICON | NIF_TIP;
            nid.hIcon = LoadIcon(Hinst, IsEnabled() 
                ? MAKEINTRESOURCE(IDI_ICON1) 
                : MAKEINTRESOURCE(IDI_ICON2));
            lstrcpy(nid.szTip, L"TouchCursor"); 
            if (!IsEnabled()) lstrcat(nid.szTip, L" (Disabled)");
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }
    }

}


INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

        case WM_INITDIALOG: {
            SetWindowText(hwnd, windowTitle);
            break;
        }

        case loadOptionsMsg:
            LoadOptions();
            break;

        case taskbarNotificationMsg: 
            // taskbar event
            switch (lParam) {
                case WM_CONTEXTMENU:
                case WM_RBUTTONDOWN:
                {
                    SetForegroundWindow(hwnd);
                    DWORD command = showMenu(hwnd);
                    switch (command) {
                        case ID_POPUP_EXIT:
                            PostQuitMessage(0);
                            break;
                        case ID_POPUP_CONFIGURE:
                            runConfigure();
                            break;
                        case ID_POPUP_HELP:
                            showHelp();
                            break;
                        default: break;
                    }
                    NOTIFYICONDATA nid = {sizeof(nid), hwnd, 0};
                    Shell_NotifyIcon(NIM_SETFOCUS, &nid);
                    break;
                }

                case WM_LBUTTONDBLCLK:
                case NIN_KEYSELECT:
                    runConfigure();
                    break;

                default: 
                    return FALSE;
            }
            break;

        // For quitting from another instance
        case ID_POPUP_EXIT:
            PostQuitMessage(0);
            break;

        case WM_TIMER:
            // Put ourselves at the front of the queue
            // so that VMWare and Virtual PC are downstream.
            ReHook();
            return TRUE; // Otherwise refreshIconState() call gives us flashing tooltip in notification area.
            break;

        default: 
            return FALSE;
    }

    refreshIconState(hwnd);
    return TRUE;
}


int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR commandLine, int nCmdShow) {
    // Re-running prompts a reload of options from registry in the original instance
    // unless parameter is "quit".
    bool quit = !lstrcmpiA(commandLine, "quit");

    // only allow one instance to run:
    CreateMutex(0, 0, L"touchcursor process");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwnd = FindWindow(0, windowTitle);
        if (hwnd) {
            if (quit) {
                PostMessage(hwnd, ID_POPUP_EXIT, 0, 0);
            }
            else {
                PostMessage(hwnd, loadOptionsMsg, 0, 0);
            }
        }
        return 0;
    }

    if (quit) return 0;

    Hinst = hInstance;
    HWND w = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc);

    InstallHook();
    refreshIconState(w);
    SetTimer(w, 1, 500, 0);

    CheckForUpdate();

    MSG msg;
    int ret = 0;
    while(ret = GetMessage(&msg, 0, 0, 0) > 0) {
        if (!IsDialogMessage(w, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    RemoveHook();
    refreshIconState(w, true);

    return ret;
}
