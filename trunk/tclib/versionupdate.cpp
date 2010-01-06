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
#include "options.h"
#include "launch.h"

#include <windows.h>
#include <wininet.h>
#include <string>
#include <sstream>

namespace {
    const int buildNum = 769;

    std::wstring versionCheckUrl() {
        std::wostringstream url;
        url << L"http://touchcursor.com/version/o/" << buildNum;
        return url.str();
    }
}

namespace tclib { 

VersionStatus GetVersionStatus() {
    VersionStatus status = dontKnow;
    HINTERNET hi = InternetOpen(L"touchcursor version check", INTERNET_OPEN_TYPE_PRECONFIG_WITH_NO_AUTOPROXY, 0, 0, 0);
    if (hi) {
        std::wstring url = versionCheckUrl();
        //const wchar_t* url = L"http://asdfadflkjdfa/";
        HINTERNET hu = InternetOpenUrl(hi, url.c_str(), 0, 0, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
        if (hu) {
            const size_t buffSize = 100;
            char buffer[buffSize];
            DWORD numRead = 0;
            if (InternetReadFile(hu, buffer, buffSize-1, &numRead)) {
                // if there's no empty file matching at this url (& got a 404), then there's a new version out
                status = (numRead != 0) ? outOfDate : upToDate;
            }
            InternetCloseHandle(hu);
        }
        InternetCloseHandle(hi);
    }
    return status;
}


void BrowseToDownload() {
    LaunchUrl(L"http://touchcursor.com/update.html");
}

} // namespace tclib