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


#ifndef MAIN_INCLUDED
#define MAIN_INCLUDED

#include <wx/wx.h>

bool EditProgList(wxString& programsInOut, wxWindow* editButton);

void AddBinding(wxListBox* keyList, int activationKeyWinCode);
void EditBinding(wxListBox* keyList, int selection, int activationKeyWinCode);
void MappingFromBox(int* keyMapping, int mappingLen, wxListBox* keyList);
void MappingToBox(const int* keyMapping, int mappingLen, wxListBox* keyList);

#endif //ndef MAIN_INCLUDED

