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


#pragma warning (disable: 4267)

#include "options.h"
#include "launch.h" // for PrefixProgramFolder()
#include "boost/archive/text_woarchive.hpp"
#include "boost/archive/text_wiarchive.hpp"
#include "boost/archive/archive_exception.hpp"
#include "boost/serialization/vector.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <ctime>

#include <windows.h>
#include <shlobj.h>

BOOST_CLASS_VERSION(Options, 6)


namespace {

    const wchar_t* const appKey = L"Software\\TouchCursor";
    const wchar_t* const settingsName = L"Settings";

    bool isInstalledOnRemovableDrive() {
        const wchar_t* cl = GetCommandLine();
        while (*cl == '\"') ++cl;
        wchar_t buff[4];
        lstrcpyn(buff, cl, 4);
        return GetDriveType(buff) == DRIVE_REMOVABLE;
    }


    const char* settingsFilePath() {
        static char buff[MAX_PATH];
        if (!*buff) { // not done it already
            wchar_t longbuff[MAX_PATH];
            if (isInstalledOnRemovableDrive()) {
                PrefixProgramFolder(longbuff, L"settings.cfg");
            }
            else {
                SHGetFolderPath(0, CSIDL_APPDATA, 0, SHGFP_TYPE_CURRENT, longbuff);
                CreateDirectory(longbuff, 0);
                lstrcat(longbuff, L"\\TouchCursor");
                CreateDirectory(longbuff, 0);
                lstrcat(longbuff, L"\\settings.cfg");
            }
            std::wcstombs(buff, longbuff, MAX_PATH);
        }
        return buff;
    }


    std::wstring readSettingsFromRegistry() {
        // now for backwards compatibility only
        HKEY key;
        LONG ret = RegOpenKeyEx(HKEY_CURRENT_USER, appKey, 0, KEY_READ, &key);
        if (ret != ERROR_SUCCESS) return L"";

        DWORD buffLenBytes = 0;
        RegQueryValueEx(key, settingsName, 0, 0, 0, &buffLenBytes);

        std::vector<wchar_t> buffer(buffLenBytes/sizeof(wchar_t));
        ret = RegQueryValueEx(key, settingsName, 0, 0, static_cast<BYTE*>(static_cast<void*>(&buffer[0])), &buffLenBytes);
        if (ret != ERROR_SUCCESS) return L"";

        buffer.pop_back(); // zero terminator
        std::wstring settings(buffer.begin(), buffer.end());

        RegCloseKey(key);
        return settings;
    }


    std::wstring readSettings() {
        std::wstring settings;
        std::wifstream file(settingsFilePath());

        if (file.is_open()) {
            std::getline(file, settings, wchar_t(0));
            return settings;
        }
        else {
            return readSettingsFromRegistry();
        }
    }


    void writeSettings(const std::wstring& settings) {
        std::wofstream file(settingsFilePath());
        file << settings;
    }


    void setDefaultMapping(int* keyMapping) {
        std::fill(keyMapping, keyMapping + Options::maxCodes, 0);

        keyMapping['I'] = VK_UP;
        keyMapping['J'] = VK_LEFT;
        keyMapping['K'] = VK_DOWN;
        keyMapping['L'] = VK_RIGHT;
        // other navigation
        keyMapping['U'] = VK_HOME;
        keyMapping['O'] = VK_END;
        keyMapping['H'] = VK_PRIOR; //page up
        keyMapping['N'] = VK_NEXT; // page down
        // insert/delete
        keyMapping['Y'] = VK_INSERT;
        keyMapping['M'] = VK_DELETE;
        keyMapping['P'] = VK_BACK;
    }
}


namespace boost {
    namespace serialization {
        template<class Archive>
        void serialize(Archive& ar, Options& opts, const unsigned int version) {
            ar  & opts.enabled
                & opts.trainingMode
                & opts.runAtStartup
                ;

            if (version >= 2) { // rev > 481
                ar & opts.showInNotificationArea;
            }
            if (version >= 3) { // rev > 662
                ar & opts.activationKey;
            }
            if (version >= 4) { // rev > 700
                ar & opts.beepForMistakes;
            }
            if (version >= 5) { // rev > 714
                ar & opts.checkForUpdates;
            }

            ar  & opts.keyMapping

                & opts.disableProgs
                & opts.enableProgs
                & opts.neverTrainProgs
                & opts.onlyTrainProgs
                & opts.useEnableList
                & opts.useOnlyTrainList
                ;

            if (version < 6) { // first open source version
                // just ignore old key stuff
            }
        }
    } 
} 

//---------------------------------------------------------------------------

Options::Options(InitOpt opt) 
    : enabled(true)
    , trainingMode(false)
    , beepForMistakes(true)
    , runAtStartup(!isInstalledOnRemovableDrive())
    , showInNotificationArea(true)
    , checkForUpdates(true)
    , activationKey(VK_SPACE)
    , useEnableList(false)  
    , useOnlyTrainList(false)
{
    setDefaultMapping(keyMapping);

    if (opt == autoload) {
        try {
            Load();
        }
        catch (std::exception&) { // More specific archive exceptions don't catch it :-S
            // First run or bad data.  
            // Store defaults.
            *this = Options(Options::defaults);
            Save();
        }
    }
}


void Options::Load() {
    std::wistringstream in(readSettings());
    boost::archive::text_wiarchive ia(in);
    ia >> *this;
}


void Options::Save() const {
    writeSettings(AsString());
}


std::wstring Options::AsString() const {
    std::wostringstream strm;
    boost::archive::text_woarchive oa(strm);
    oa << *this;
    return strm.str();
}


bool Options::ShouldCheckForUpdate() const {
    const time_t secondsPerDay = 60*60*24;
    int timeInDays = static_cast<int>(time(0) / secondsPerDay);

    // Lazy: removed this value along with registration stuff, so everybody now checks on the same day of the week.
    const int firstRunDay = 0;

    const int checkFrequencyDays = 7;
    return checkForUpdates && (timeInDays - firstRunDay) % checkFrequencyDays == 0;
}