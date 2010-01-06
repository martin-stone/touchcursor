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


#ifndef TOUCHCURSOR_LAUNCH
#define TOUCHCURSOR_LAUNCH

// Get folder of this running program.  Buff must be big enough.
void GetProgramFolder(wchar_t* buff);

void PrefixProgramFolder(wchar_t* buff, const wchar_t* path);

bool LaunchProgRelative(const wchar_t* prog, const wchar_t* args=L"");
void LaunchLocalHtml(const wchar_t* filename);
void LaunchUrl(const wchar_t* url);

#endif //ndef TOUCHCURSOR_LAUNCH

