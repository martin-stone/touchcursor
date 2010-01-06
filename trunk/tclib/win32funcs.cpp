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


#define _WIN32_WINNT 0x0500 // for VK_VOLUME_UP etc
#include "win32funcs.h"
#include <windows.h>
#include <psapi.h>


namespace {
    static HWND lastWindow = 0;

    HWND windowUnder(int x, int y) {
        POINT p = {x, y};
        HWND w = WindowFromPoint(p);
        return GetAncestor(w, GA_ROOTOWNER);
    }

    void drawWindowOutline(HWND w) {
        HDC dc = GetWindowDC(w);
        int oldRop = SetROP2(dc, R2_NOT);
        int borderW = GetSystemMetrics(SM_CXFRAME);
        int borderH = GetSystemMetrics(SM_CYFRAME);
            
        HRGN region = CreateRectRgn(0,0,0,0);
        if (GetWindowRgn(w, region) == ERROR) { 
            // classic style windows don't have regions
            RECT rect;
            GetWindowRect(w, &rect);
            if (IsZoomed(w)) {
                InflateRect(&rect, -borderW, -borderH);
                OffsetRect(&rect, borderW, borderH);
            }
            else {
                OffsetRect(&rect, -rect.left, -rect.top);
            }
            SetRectRgn(region, rect.left, rect.top, rect.right, rect.bottom);
        }
        FrameRgn(dc, region, (HBRUSH)GetStockObject(BLACK_BRUSH), borderW, borderH);
        SetROP2(dc, oldRop);
        ReleaseDC(w, dc);
        DeleteObject(region);
    }

    void removeWindowOutline(HWND w) {
        if (w) {
            RedrawWindow(w, 0, 0, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }

    bool instanceExists(const wchar_t* title) {
        CreateMutex(0, 0, title);
        return GetLastError() == ERROR_ALREADY_EXISTS;
    }


    DWORD getProcessImageBaseName(HANDLE process, wchar_t* buff, size_t numBuffBytes) {
        HMODULE exeModule;
        DWORD bytesNeeded; // don't care
        // We ASSUME that the first module enumerated is the exe (seems to work)
        EnumProcessModules(process, &exeModule, sizeof(exeModule), &bytesNeeded);
        return GetModuleBaseName(process, exeModule, buff, static_cast<DWORD>(numBuffBytes)); 
    }

    int discriminateLeftRight(int vkEither, int vkLeft, int vkRight) {
        // only works on keydown
        int left = GetKeyState(vkLeft) & 0x80;
        int right = GetKeyState(vkRight) & 0x80;

        if (left && !right) return vkLeft;
        else if (right && !left) return vkRight;
        else return vkEither; // can't tell
    }
}

namespace win32funcs {

    void GetWindowExeName(wchar_t* buff, size_t buffSize, HWND window) {
        DWORD id;
        GetWindowThreadProcessId(window, &id);
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, id);
        getProcessImageBaseName(process, buff, buffSize * sizeof(*buff));
        CloseHandle(process);
    }


    std::wstring GetExeForMousePos(int x, int y) {
        HWND w = windowUnder(x, y);
        removeWindowOutline(w);
        wchar_t buff[MAX_PATH];
        GetWindowExeName(buff, MAX_PATH, w);
        return buff;
    }


    void HighlightWindow(int x, int y) {
        HWND w = windowUnder(x, y);
        if (w != lastWindow) {
            removeWindowOutline(lastWindow);
            drawWindowOutline(w);
            lastWindow = w;
        }
    }

    void UnHighlightWindow() {
        removeWindowOutline(lastWindow);
    }

    void QuitExistingWindow(const wchar_t* title) {
        if (instanceExists(title)) {
            HWND w = FindWindow(0, title);
            if (w) PostMessage(w, WM_CLOSE, 0, 0);
        }
    }

    bool RaiseExistingWindow(const wchar_t* title) {
        if (instanceExists(title)) {
            HWND w = FindWindow(0, title);
            if (w) SetForegroundWindow(w);
            return true;
        }
        else return false;
    }


    int DiscriminateLeftRightModifierJustPressed(int vkCode) {
        switch (vkCode) {
            case VK_SHIFT: 
                return discriminateLeftRight(vkCode, VK_LSHIFT, VK_RSHIFT);
            case VK_CONTROL: 
                return discriminateLeftRight(vkCode, VK_LCONTROL, VK_RCONTROL);
            default: return vkCode;
        }
    }


    const wchar_t* VkCodeToStr(int vkCode) {
        UINT c = MapVirtualKey(vkCode, 2) & 0xff;
        if (c && iswprint(c) && !iswspace(c)) {
            static wchar_t buff[2]; //XXX bleuch!
            buff[0] = c;
            return buff;
        }
        else switch (vkCode) {
            #define MAP_KEY_CODE_TO_STR(VK, STRING) case VK: return L##STRING;

            //MAP_KEY_CODE_TO_STR(VK_LBUTTON, "L Button")
            //MAP_KEY_CODE_TO_STR(VK_RBUTTON, "R Button")
            MAP_KEY_CODE_TO_STR(VK_CANCEL, "Cancel")
            //MAP_KEY_CODE_TO_STR(VK_MBUTTON, "M Button")
            #if(_WIN32_WINNT >= 0x0500)
            //MAP_KEY_CODE_TO_STR(VK_XBUTTON1, "X Button 1")
            //MAP_KEY_CODE_TO_STR(VK_XBUTTON2, "X Button 2")
            #endif /* _WIN32_WINNT >= 0x0500 */

            MAP_KEY_CODE_TO_STR(VK_BACK, "Back")
            MAP_KEY_CODE_TO_STR(VK_TAB, "Tab")

            MAP_KEY_CODE_TO_STR(VK_CLEAR, "Clear")
            MAP_KEY_CODE_TO_STR(VK_RETURN, "Return")

            MAP_KEY_CODE_TO_STR(VK_SHIFT, "Shift")
            MAP_KEY_CODE_TO_STR(VK_CONTROL, "Control")
            //MAP_KEY_CODE_TO_STR(VK_MENU, "Alt")
            MAP_KEY_CODE_TO_STR(VK_PAUSE, "Pause/Break")
            MAP_KEY_CODE_TO_STR(VK_CAPITAL, "Caps Lock")

            //MAP_KEY_CODE_TO_STR(VK_KANA, "KANA")
            //MAP_KEY_CODE_TO_STR(VK_HANGEUL, "HANGEUL")
            MAP_KEY_CODE_TO_STR(VK_HANGUL, "Hangul")
            MAP_KEY_CODE_TO_STR(VK_JUNJA, "Junja")
            MAP_KEY_CODE_TO_STR(VK_FINAL, "Final")
            //MAP_KEY_CODE_TO_STR(VK_HANJA, "HANJA")
            MAP_KEY_CODE_TO_STR(VK_KANJI, "Kanji")

            MAP_KEY_CODE_TO_STR(VK_ESCAPE, "Escape")

            MAP_KEY_CODE_TO_STR(VK_CONVERT, "Convert")
            MAP_KEY_CODE_TO_STR(VK_NONCONVERT, "Non Convert")
            MAP_KEY_CODE_TO_STR(VK_ACCEPT, "Accept")
            MAP_KEY_CODE_TO_STR(VK_MODECHANGE, "Modechange")

            MAP_KEY_CODE_TO_STR(VK_SPACE, "Space")
            MAP_KEY_CODE_TO_STR(VK_PRIOR, "Page Up")
            MAP_KEY_CODE_TO_STR(VK_NEXT, "Page Down")
            MAP_KEY_CODE_TO_STR(VK_END, "End")
            MAP_KEY_CODE_TO_STR(VK_HOME, "Home")
            MAP_KEY_CODE_TO_STR(VK_LEFT, "Left")
            MAP_KEY_CODE_TO_STR(VK_UP, "Up")
            MAP_KEY_CODE_TO_STR(VK_RIGHT, "Right")
            MAP_KEY_CODE_TO_STR(VK_DOWN, "Down")
            MAP_KEY_CODE_TO_STR(VK_SELECT, "Select")
            MAP_KEY_CODE_TO_STR(VK_PRINT, "Print")
            MAP_KEY_CODE_TO_STR(VK_EXECUTE, "Execute")
            MAP_KEY_CODE_TO_STR(VK_SNAPSHOT, "Snapshot")
            MAP_KEY_CODE_TO_STR(VK_INSERT, "Insert")
            MAP_KEY_CODE_TO_STR(VK_DELETE, "Delete")
            MAP_KEY_CODE_TO_STR(VK_HELP, "Help")

            MAP_KEY_CODE_TO_STR(VK_LWIN, "Left Win")
            MAP_KEY_CODE_TO_STR(VK_RWIN, "Right Win")
            MAP_KEY_CODE_TO_STR(VK_APPS, "Apps")

            MAP_KEY_CODE_TO_STR(VK_SLEEP, "Sleep")

            MAP_KEY_CODE_TO_STR(VK_NUMPAD0, "Numpad 0")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD1, "Numpad 1")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD2, "Numpad 2")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD3, "Numpad 3")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD4, "Numpad 4")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD5, "Numpad 5")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD6, "Numpad 6")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD7, "Numpad 7")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD8, "Numpad 8")
            MAP_KEY_CODE_TO_STR(VK_NUMPAD9, "Numpad 9")
            MAP_KEY_CODE_TO_STR(VK_MULTIPLY, "Multiply")
            MAP_KEY_CODE_TO_STR(VK_ADD, "Add")
            MAP_KEY_CODE_TO_STR(VK_SEPARATOR, "Separator")
            MAP_KEY_CODE_TO_STR(VK_SUBTRACT, "Subtract")
            MAP_KEY_CODE_TO_STR(VK_DECIMAL, "Decimal")
            MAP_KEY_CODE_TO_STR(VK_DIVIDE, "Divide")
            MAP_KEY_CODE_TO_STR(VK_F1, "F1")
            MAP_KEY_CODE_TO_STR(VK_F2, "F2")
            MAP_KEY_CODE_TO_STR(VK_F3, "F3")
            MAP_KEY_CODE_TO_STR(VK_F4, "F4")
            MAP_KEY_CODE_TO_STR(VK_F5, "F5")
            MAP_KEY_CODE_TO_STR(VK_F6, "F6")
            MAP_KEY_CODE_TO_STR(VK_F7, "F7")
            MAP_KEY_CODE_TO_STR(VK_F8, "F8")
            MAP_KEY_CODE_TO_STR(VK_F9, "F9")
            MAP_KEY_CODE_TO_STR(VK_F10, "F10")
            MAP_KEY_CODE_TO_STR(VK_F11, "F11")
            MAP_KEY_CODE_TO_STR(VK_F12, "F12")
            MAP_KEY_CODE_TO_STR(VK_F13, "F13")
            MAP_KEY_CODE_TO_STR(VK_F14, "F14")
            MAP_KEY_CODE_TO_STR(VK_F15, "F15")
            MAP_KEY_CODE_TO_STR(VK_F16, "F16")
            MAP_KEY_CODE_TO_STR(VK_F17, "F17")
            MAP_KEY_CODE_TO_STR(VK_F18, "F18")
            MAP_KEY_CODE_TO_STR(VK_F19, "F19")
            MAP_KEY_CODE_TO_STR(VK_F20, "F20")
            MAP_KEY_CODE_TO_STR(VK_F21, "F21")
            MAP_KEY_CODE_TO_STR(VK_F22, "F22")
            MAP_KEY_CODE_TO_STR(VK_F23, "F23")
            MAP_KEY_CODE_TO_STR(VK_F24, "F24")

            MAP_KEY_CODE_TO_STR(VK_NUMLOCK, "Num Lock")
            MAP_KEY_CODE_TO_STR(VK_SCROLL, "Scroll Lock")

            MAP_KEY_CODE_TO_STR(VK_OEM_NEC_EQUAL, "OEM NEC Equal")

            //MAP_KEY_CODE_TO_STR(VK_OEM_FJ_JISHO, "OEM_FJ_JISHO")
            MAP_KEY_CODE_TO_STR(VK_OEM_FJ_MASSHOU, "OEM FJ Masshou")
            MAP_KEY_CODE_TO_STR(VK_OEM_FJ_TOUROKU, "OEM FJ Touroku")
            MAP_KEY_CODE_TO_STR(VK_OEM_FJ_LOYA, "OEM FJ Loya")
            MAP_KEY_CODE_TO_STR(VK_OEM_FJ_ROYA, "OEM FJ Roya")

            MAP_KEY_CODE_TO_STR(VK_LSHIFT, "Left Shift")
            MAP_KEY_CODE_TO_STR(VK_RSHIFT, "Right Shift")
            MAP_KEY_CODE_TO_STR(VK_LCONTROL, "Left Control")
            MAP_KEY_CODE_TO_STR(VK_RCONTROL, "Right Control")
            MAP_KEY_CODE_TO_STR(VK_LMENU, "Left Alt")
            MAP_KEY_CODE_TO_STR(VK_RMENU, "Right Alt")

            #if(_WIN32_WINNT >= 0x0500)
            MAP_KEY_CODE_TO_STR(VK_BROWSER_BACK, "Browser Back")
            MAP_KEY_CODE_TO_STR(VK_BROWSER_FORWARD, "Browser Forward")
            MAP_KEY_CODE_TO_STR(VK_BROWSER_REFRESH, "Browser Refresh")
            MAP_KEY_CODE_TO_STR(VK_BROWSER_STOP, "Browser Stop")
            MAP_KEY_CODE_TO_STR(VK_BROWSER_SEARCH, "Browser Search")
            MAP_KEY_CODE_TO_STR(VK_BROWSER_FAVORITES, "Browser Favorites")
            MAP_KEY_CODE_TO_STR(VK_BROWSER_HOME, "Browser Home")

            MAP_KEY_CODE_TO_STR(VK_VOLUME_MUTE, "Volume Mute")
            MAP_KEY_CODE_TO_STR(VK_VOLUME_DOWN, "Volume Down")
            MAP_KEY_CODE_TO_STR(VK_VOLUME_UP, "Volume Up")
            MAP_KEY_CODE_TO_STR(VK_MEDIA_NEXT_TRACK, "Media Next Track")
            MAP_KEY_CODE_TO_STR(VK_MEDIA_PREV_TRACK, "Media Prev Track")
            MAP_KEY_CODE_TO_STR(VK_MEDIA_STOP, "Media Stop")
            MAP_KEY_CODE_TO_STR(VK_MEDIA_PLAY_PAUSE, "Media Play/Pause")
            MAP_KEY_CODE_TO_STR(VK_LAUNCH_MAIL, "Launch Mail")
            MAP_KEY_CODE_TO_STR(VK_LAUNCH_MEDIA_SELECT, "Launch Media Select")
            MAP_KEY_CODE_TO_STR(VK_LAUNCH_APP1, "Launch App 1")
            MAP_KEY_CODE_TO_STR(VK_LAUNCH_APP2, "Launch App 2")
            #endif /* _WIN32_WINNT >= 0x0500 */

            // OEM_* Should be handled by MapVirtualKey
            MAP_KEY_CODE_TO_STR(VK_OEM_1, "OEM 1")
            MAP_KEY_CODE_TO_STR(VK_OEM_PLUS, "OEM Plus")
            MAP_KEY_CODE_TO_STR(VK_OEM_COMMA, "OEM Comma")
            MAP_KEY_CODE_TO_STR(VK_OEM_MINUS, "OEM Minus")
            MAP_KEY_CODE_TO_STR(VK_OEM_PERIOD, "OEM Period")
            MAP_KEY_CODE_TO_STR(VK_OEM_2, "OEM 2")
            MAP_KEY_CODE_TO_STR(VK_OEM_3, "OEM 3")

            MAP_KEY_CODE_TO_STR(VK_OEM_4, "OEM 4")
            MAP_KEY_CODE_TO_STR(VK_OEM_5, "OEM 5")
            MAP_KEY_CODE_TO_STR(VK_OEM_6, "OEM 6")
            MAP_KEY_CODE_TO_STR(VK_OEM_7, "OEM 7")
            MAP_KEY_CODE_TO_STR(VK_OEM_8, "OEM 8")

            MAP_KEY_CODE_TO_STR(VK_OEM_AX, "OEM AX")
            MAP_KEY_CODE_TO_STR(VK_OEM_102, "OEM 102")
            MAP_KEY_CODE_TO_STR(VK_ICO_HELP, "Ico Help")
            MAP_KEY_CODE_TO_STR(VK_ICO_00, "Ico 00")

            #if(WINVER >= 0x0400)
            MAP_KEY_CODE_TO_STR(VK_PROCESSKEY, "Process")
            #endif /* WINVER >= 0x0400 */

            MAP_KEY_CODE_TO_STR(VK_ICO_CLEAR, "Ico Clear")

            #if(_WIN32_WINNT >= 0x0500)
            //MAP_KEY_CODE_TO_STR(VK_VK_PACKET, "Packet")
            #endif /* _WIN32_WINNT >= 0x0500 */

            default: return L"";
        }
    }


    bool IsModifierKey(int code) {
        return code == VK_SHIFT
            || code == VK_CONTROL
            || code == VK_MENU
            || code == VK_LSHIFT
            || code == VK_RSHIFT
            || code == VK_LCONTROL
            || code == VK_RCONTROL
            || code == VK_LMENU
            || code == VK_RMENU
            || code == VK_LWIN
            || code == VK_RWIN
            ;
    }
}
