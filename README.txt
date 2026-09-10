TouchCursor Readme
==================

Dependencies
============

C++ Libraries
-------------

* wxWidgets 3.2 (static unicode 32-bit / x86 build)
* Boost 1.86 (x86 MSVC prebuilt binaries; requires compiled boost_serialization)

The location of these libraries can be configured by editing 
touchcursor.props.

Tools
-----

* Microsoft Visual Studio 2022 or 2026 (Community or higher)
  with the "Desktop development with C++" workload (x86 / Win32 toolset)

Scripts that build the installer and docs require:

* InnoSetup >=v5.2.2 (http://www.jrsoftware.org/isinfo.php)
* Python >=v2.4 (http://python.org)
* Python markdown library (http://pypi.python.org/pypi/Markdown)
* 7zip (http://www.7-zip.org/)


Building
========

Run build.bat to build executables and docs and package them into zip and 
installer files. Alternatively, load touchcursor.sln into Visual Studio and 
build in the IDE, or use MSBuild directly from the command line:

    msbuild touchcursor.sln /p:Configuration=Release /p:Platform=Win32


Project Directories
===================

touchcursor
    Source for touchcursor.exe, which loads the hook DLL and runs the systray 
    menu.

touchcursordll
    Source for touchcursor.dll. This is the core of TouchCursor and contains 
    the keyboard hook function. (Windows requires this to be in a DLL rather than
    an exe.)

touchcursorconfig
    Source for tcconfig.exe, which edits the configuration file. This was 
    implemented as separate program to keep the memory usage of the permanently
    running modules low.

touchcursor_update
    Source for a touchcursor_update.exe, which is optionally used to check for a
    new version of TouchCursor. The separate exe is partly due to paranoia about
    malware scanners that might be suspicious of a program that installed a 
    keyboard hook and connected to the internet.

tclib
    Functions shared by the other projects.

setup
    Source, scripts and dependencies for building the installer.

docs
    Source for the help file. The text is in Markdown format. The html is built
    by a script in the setup directory.


License & Copyright
===================

Copyright © 2010 Martin Stone.

TouchCursor is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

TouchCursor is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with TouchCursor (COPYING.txt).  If not, see 
<http://www.gnu.org/licenses/>.
