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


#include "versionupdate.h"

#include "resource.h"
#include <windows.h>

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

        case WM_INITDIALOG: {
            break;
        }

        case WM_COMMAND:
            if (IsWindowVisible(hwnd)) {
                switch (wParam & 0xffff) {
                case IDYES: {
                    tclib::BrowseToDownload();
                    PostQuitMessage(0);
                    break;
                }
                case IDNO:
                case IDCANCEL:
                    PostQuitMessage(0);
                    break;
                }
            }
            break;

        default: 
            return FALSE;
    }

    return TRUE;
}


int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR commandLine, int nCmdShow) {

    // only allow one instance to run:
    CreateMutex(0, 0, L"touchcursor update process");
    if (GetLastError() != ERROR_ALREADY_EXISTS) {

        // don't slow down PC startup -- wait
        const int waitMins = 3;
        Sleep(waitMins * 60 * 1000);
        
        if (tclib::GetVersionStatus() == tclib::outOfDate) {
            HWND w = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc);
            SendMessage(w, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0)));
            SendMessage(w, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0)));
            SetForegroundWindow(w);

            MSG msg;
            while(GetMessage(&msg, 0, 0, 0) > 0) {
                if (!IsDialogMessage(w, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }

    return 0;
}

