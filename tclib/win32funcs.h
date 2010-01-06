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


#ifndef WIN32FUNCS_INCLUDED
#define WIN32FUNCS_INCLUDED

#pragma comment(lib, "psapi.lib")

#include <string>

namespace win32funcs {
    
    std::wstring GetExeForMousePos(int x, int y);
    void HighlightWindow(int x, int y); 
    void UnHighlightWindow();
    void QuitExistingWindow(const wchar_t* title);
    bool RaiseExistingWindow(const wchar_t* title);
    const wchar_t* VkCodeToStr(int vkCode);
    bool IsModifierKey(int vkCode);
    int DiscriminateLeftRightModifierJustPressed(int vkCode);
}


#endif //ndef WIN32FUNCS_INCLUDED