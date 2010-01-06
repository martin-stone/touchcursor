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


#include "launch.h"
#include <windows.h>

void GetProgramFolder(wchar_t* buff) {
    lstrcpy(buff, GetCommandLine());

    // get rid of "
    wchar_t* start = buff;
    while (*start == '\"') ++start;
    if (start != buff) {
        wchar_t* dest = buff;
        do {
            *dest++ = *start++;
        } while (*start);
    }

    wchar_t* end = buff + lstrlen(buff);
    while (*end != '\\' && end > buff) *end-- = 0;
    *end = 0;
}


void PrefixProgramFolder(wchar_t* buff, const wchar_t* path) {
    GetProgramFolder(buff);
    lstrcat(buff, L"\\");
    lstrcat(buff, path);
}


bool LaunchProgRelative(const wchar_t* prog, const wchar_t* args) {
    wchar_t command[2*1024] = L"\"";
    PrefixProgramFolder(command+1, prog);
    lstrcat(command, L"\"");

    if (*args) {
        lstrcat(command, L" ");
        lstrcat(command, args);
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    BOOL ret = CreateProcess(0, command, 0, 0, 0, 0, 0, 0, &si, &pi);
    return ret;
}


void LaunchUrl(const wchar_t* url) {
    ShellExecute(0, L"open", url, 0, L"", SW_NORMAL);
}


void LaunchLocalHtml(const wchar_t* filename) {
    wchar_t path[MAX_PATH];
    if (filename[0] == L'\\' || filename[1] == L':') {
        lstrcpy(path, filename);
    }
    else {
        // relative: Make absolute
        PrefixProgramFolder(path, filename);
    }

    LaunchUrl(path);
}
