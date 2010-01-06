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


#ifndef REGISTRY_OPTIONS
#define REGISTRY_OPTIONS

#include <vector>
#include <string>

struct Options {
    enum InitOpt {autoload, defaults};
    Options(InitOpt opt=autoload);

    void Load();
    void Save() const;
    std::wstring AsString() const;

    bool ShouldCheckForUpdate() const;

    // General
    bool enabled;
    bool trainingMode;
    bool beepForMistakes;
    bool runAtStartup;
    bool showInNotificationArea;
    bool checkForUpdates;
    int activationKey;

    // Key mapping
    static const int maxCodes = 0x100;
    int keyMapping[maxCodes];

    // Programs
    typedef std::vector<std::wstring> StringVec;
    StringVec disableProgs;
    StringVec enableProgs;
    StringVec neverTrainProgs;
    StringVec onlyTrainProgs;
    bool useEnableList;
    bool useOnlyTrainList;
};


#endif //ndef REGISTRY_OPTIONS