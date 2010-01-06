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


#define _WIN32_WINNT 0x0500 // for WH_KEYBOARD_LL, SendInput()
#include "options.h"
#include "win32funcs.h"
#include "launch.h"
#include "modifiers.h"

#include <algorithm>

#include <windows.h>
#include <cstdio>
#include <cassert>

#ifndef NDEBUG
#define UNIT_TEST
#endif


namespace win32funcs {
    // not declared in header so that windows.h isn't needed
    void GetWindowExeName(wchar_t* buff, size_t buffSize, HWND window);
}

namespace {
    static Options options;
    static HHOOK HookHandle = 0;
    static DWORD savedKeyDown = 0;
    static bool hadKeypressSinceLastTick = false;

    // Must keep track of what is held, so that we can do key-ups for 
    // them all when space is released.
    static bool mappedKeysHeld[Options::maxCodes];

    static DWORD modifierState = 0;

    const size_t maxModCodes = 3;
    struct ModMapping {
        DWORD bit; 
        WORD codes[maxModCodes];
    };
    const ModMapping modifierTable[] = {
        {shiftFlag, {VK_SHIFT, VK_LSHIFT, VK_RSHIFT}},
        {ctrlFlag,  {VK_CONTROL, VK_LCONTROL, VK_RCONTROL}},
        {altFlag,   {VK_MENU, VK_LMENU, VK_RMENU}},
        {winFlag,   {VK_LWIN, 0, 0}},
        {0}
    };

    const DWORD configKey = VK_F5;

    // make an unlikely flag value for identifying our injected messages
    const ULONG_PTR injectedFlag = 'TCUR';


    bool runConfigure(DWORD) {
        LaunchProgRelative(L"tcconfig.exe");
        return true;
    }
}


//---------------------------------------------------------------------------
#ifdef UNIT_TEST
//---------------------------------------------------------------------------

namespace test {
    static bool isInUnitTest = false;
    const size_t logOutputSize = 1000;
    static DWORD keyOutput[logOutputSize];
    static DWORD* outputPos = keyOutput;

    void resetOutput() {
        memset(keyOutput, 0, sizeof(keyOutput));
        outputPos = keyOutput;
    }

    // For tests: Wrap the real functions with these logging versions:

    LRESULT loggedCallNextHookEx(HHOOK hhk, int nCode, WPARAM wParam, LPARAM lParam) {
        if (isInUnitTest) {
            if (nCode >= 0) {
                KBDLLHOOKSTRUCT* h = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                *outputPos++ = h->vkCode;
                *outputPos++ = h->flags;
            }
            return 1;
        }
        else {
            return CallNextHookEx(HookHandle, nCode, wParam, lParam); 
        }
    }

    UINT loggedSendInput(UINT nInputs, LPINPUT pInputs, int cbSize) {
        if (isInUnitTest) {
            for (UINT i=0; i<nInputs; ++i) {
                *outputPos++ = pInputs[i].ki.wVk;
                *outputPos++ = pInputs[i].ki.dwFlags;
            }
            return 1;
        }
        else {
            return SendInput(nInputs, pInputs, cbSize);
        }
    }

    bool loggedRunConfigure(DWORD code) {
        if (isInUnitTest) {
            *outputPos++ = '*';
            *outputPos++ = 0;
            return true;
        }
        else {
            return runConfigure(code);
        }
    }
}

// must be below definitions otherwise stack overflow
#define CallNextHookEx test::loggedCallNextHookEx
#define SendInput test::loggedSendInput
#define runConfigure test::loggedRunConfigure

//---------------------------------------------------------------------------
#endif //def UNIT_TEST
//---------------------------------------------------------------------------

namespace {

    bool injectedByUs(const KBDLLHOOKSTRUCT* h) {
        return h->flags & LLKHF_INJECTED && h->dwExtraInfo == injectedFlag;
    }

    bool isExtendedKey(DWORD code) {
        const DWORD extended[] = {
            VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, 
            VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_SNAPSHOT, 
            VK_INSERT, VK_DELETE, VK_DIVIDE, VK_NUMLOCK, 
            VK_RCONTROL, VK_RMENU, 
            0
        };
        for (const DWORD* p=extended; *p; ++p) {
            if (code == *p) return true;
        }
        return false;
    }

    void logModifierState(DWORD code, bool keyDown) {
        for (const ModMapping* mm=modifierTable; mm->bit; ++mm) {
            const WORD* end = mm->codes + maxModCodes;
            if (std::find(mm->codes, end, code) != end) {
                if (keyDown) modifierState |= mm->bit;
                else modifierState &= ~mm->bit;
            }
        }
    }

    void generateKeyEvent(WORD code, bool up) {
        DWORD flags = 0;
        if (up) flags |= KEYEVENTF_KEYUP;
        if (isExtendedKey(code)) flags |= KEYEVENTF_EXTENDEDKEY;

        INPUT input;
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = code;
        input.ki.wScan = MapVirtualKey(code, 0);
        input.ki.dwFlags = flags;
        input.ki.time = 0;
        input.ki.dwExtraInfo = injectedFlag;
        SendInput(1, &input, sizeof(input));

        // Sleep(1) seems to be necessary for mapped Escape events sent to VirtualPC & recent VMware versions.
        // (Sleep(0) is no good)
        // Also for mapped modifiers with Remote Desktop Connection.
        // Dunno why:
        Sleep(1); 
    }

    void keyEvent(DWORD code, bool up) {
        // separate events are not sent in one SendInput() call, because that doesn't work with Remote Desktop Connection
        DWORD modifiers = code & ~modifierState;

        if (!up) { // only add modifiers on key-down 
            for (const ModMapping* mm=modifierTable; mm->bit; ++mm) {
                if (modifiers & mm->bit) generateKeyEvent(mm->codes[0], false);
            }
        }
        generateKeyEvent(static_cast<WORD>(code & 0xff), up);
        if (!up) {
            for (const ModMapping* mm=modifierTable; mm->bit; ++mm) {
                if (modifiers & mm->bit) generateKeyEvent(mm->codes[0], true);
            }
        }
    }

    void keyDownEvent(DWORD code) {
        keyEvent(code, false);
    }

    void keyUpEvent(DWORD code) {
        keyEvent(code, true);
    }

    void tapKey(DWORD code) {
        keyDownEvent(code);
        keyUpEvent(code);
    }

    static DWORD translateCode(DWORD code) {
        if (code >= options.maxCodes) return 0;
        return options.keyMapping[code];
    }

    static bool allowedInTrainingMode(DWORD code) {
        if (code == options.activationKey) {
            // still must be allowed even if it's mapped to a key
            return true;
        }
        else {
            // not allowed if found in our key mapping
            const int* begin = options.keyMapping;
            const int* end = options.keyMapping + options.maxCodes;
            if (std::find(begin, end, code) == end) {
                // Not mapped unmodified. Check for modified with current modifiers
                return std::find(begin, end, code|modifierState) == end;
            }
            else {
                return false;
            }
        }
    }

    bool mapKey(DWORD code, bool up) {
        DWORD newCode = translateCode(code);
        if (newCode) {
            mappedKeysHeld[newCode & 0xff] = !up;
            keyEvent(newCode, up);
            return true;
        }
        else {
            return false;
        }
    }

    bool discardKey(DWORD) {
        return true;
    }

    bool discardKeyAndReleaseMappedKeys(DWORD) {
        for (int i=0; i<Options::maxCodes; ++i) {
            if (mappedKeysHeld[i]) {
                mappedKeysHeld[i] = false;
                keyEvent(i, true);
            }
        }
        return true;
    }

    bool tapActivationKey(DWORD) {
        tapKey(options.activationKey);
        return true;
    }

    bool activationKeyDownThenKey(DWORD code) {
        keyDownEvent(options.activationKey);
        keyDownEvent(code);
        return true;
    }

    bool mapKeyDown(DWORD code) {
        return mapKey(code, false);
    }

    bool mapKeyUp(DWORD code) {
        return mapKey(code, true);
    }

    bool delayKeyDown(DWORD code) {
        assert(savedKeyDown == 0);
        savedKeyDown = code;
        return true;
    }

    bool emitSaved(DWORD) {
        if (savedKeyDown) {
            keyDownEvent(savedKeyDown);
            savedKeyDown = 0;
        }
        return true;
    }

    bool emitActDownSavedDownActUp(DWORD code) {
        keyDownEvent(options.activationKey);
        emitSaved(code);
        keyUpEvent(options.activationKey);
        return true;
    }

    bool emitSavedDownAndActUp(DWORD code) {
        emitSaved(code);
        keyUpEvent(options.activationKey);
        return true;
    }

    void mapSavedDown() {
        if (savedKeyDown) {
            mapKeyDown(savedKeyDown);
            savedKeyDown = 0;
        }
    }

    bool mapSavedAndMapCurrentDown(DWORD code) {
        mapSavedDown();
        mapKeyDown(code);
        return true;
    }

    bool mapSavedAndMapCurrentUp(DWORD code) {
        mapSavedDown();
        mapKeyUp(code);
        return true;
    }

    bool emitActSavedAndCurrentDown(DWORD code) {
        keyDownEvent(options.activationKey);
        emitSaved(code);
        keyDownEvent(code);
        return true;
    }

    bool emitActSavedAndCurrentUp(DWORD code) {
        keyDownEvent(options.activationKey);
        emitSaved(code);
        keyUpEvent(code);
        return true;
    }

    bool emitSavedAndCurrentDown(DWORD code) {
        emitSaved(code);
        keyDownEvent(code);
        return true;
    }


    bool progIsInList(const wchar_t* basename, const Options::StringVec& progList) {
        for (Options::StringVec::const_iterator i=progList.begin(); i!=progList.end(); ++i) {
            if (!_wcsicmp(i->c_str(), basename)) return true;
        }
        return false;
    }


    bool trainingEnabledFor(const wchar_t* exeName) {
         if (options.useOnlyTrainList) {
            return progIsInList(exeName, options.onlyTrainProgs);
        }
        else {
            return !progIsInList(exeName, options.neverTrainProgs);
        }
    }

    bool mappingEnabledFor(const wchar_t* exeName) {
         if (options.useEnableList) {
            return progIsInList(exeName, options.enableProgs);
        }
        else {
            return !progIsInList(exeName, options.disableProgs);
        }
    }


    bool isKeyDown(WPARAM wParam) {
        return wParam == WM_KEYDOWN || wParam==WM_SYSKEYDOWN;
    }


    class StateMachine {
    public:
        StateMachine() : state(idle) {}

        void Reset() {
            state = idle;
            savedKeyDown = 0;
        }

        // returns true if key should be discarded
        bool ProcessKey(WPARAM wParam, DWORD code) {
            Event e = numEvents;
            if (code == options.activationKey) {
                e = isKeyDown(wParam) ? activationDown : activationUp;
            }
            else if (code == configKey && isKeyDown(wParam)) {
                e = configKeyDown;
            }
            else if (translateCode(code))
            {
                e = isKeyDown(wParam) ? mappedKeyDown : mappedKeyUp;
            }
            else {
                e = isKeyDown(wParam) ? otherKeyDown : otherKeyUp;
            }
            return processEvent(e, code);
        }


    private:
        enum Event {activationDown, activationUp, mappedKeyDown, mappedKeyUp, otherKeyDown, otherKeyUp, configKeyDown, numEvents};
        enum State {idle, waitMappedDown, waitMappedDownSpaceEmitted, waitMappedUp, waitMappedUpSpaceEmitted, mapping, numStates, self, na};
        typedef bool (*ActionFunc)(DWORD code);

        struct Transition {
            Event event;
            State nextState;
            ActionFunc action;
        };

        bool processEvent(Event e, DWORD code) {
            const Transition& t = transitionTable[state][e];
            assert(e == t.event);

            #ifdef UNIT_TEST
                coverage[state][e] = true;
            #endif

            if (t.nextState != self) {
                state = t.nextState;
            }

            //wchar_t buff[100];
            //wsprintf(buff, L"%i->%i  %x\n", e, state, code);
            //OutputDebugString(buff);

            return t.action ? t.action(code) : false;
        }


    #ifdef UNIT_TEST
        public:
            void printTransitionCoverage(State s, Event e, wchar_t* name) {
                if (!coverage[s][e]) {
                    OutputDebugString(name);
                    OutputDebugString(L" missed\n");
                }
            }

            void printStateCoverage(State s, wchar_t* name) {
                OutputDebugString(name);
                OutputDebugString(L"\n");
                #define PRINT_TRANSITION_COVERAGE(EVENT) printTransitionCoverage(s, EVENT, L"    " L#EVENT)                
                PRINT_TRANSITION_COVERAGE(activationDown);
                PRINT_TRANSITION_COVERAGE(activationUp);
                PRINT_TRANSITION_COVERAGE(mappedKeyDown);
                PRINT_TRANSITION_COVERAGE(mappedKeyUp);
                PRINT_TRANSITION_COVERAGE(otherKeyDown);
                PRINT_TRANSITION_COVERAGE(otherKeyUp);
                PRINT_TRANSITION_COVERAGE(configKeyDown);
            }

            void printUnusedTransitions() {
                OutputDebugString(L"Coverage report:\n");
                #define PRINT_STATE_COVERAGE(STATE) printStateCoverage(STATE, L"  " L#STATE)
                PRINT_STATE_COVERAGE(idle);
                PRINT_STATE_COVERAGE(waitMappedDown);
                PRINT_STATE_COVERAGE(waitMappedDownSpaceEmitted);
                PRINT_STATE_COVERAGE(waitMappedUp);
                PRINT_STATE_COVERAGE(waitMappedUpSpaceEmitted);
                PRINT_STATE_COVERAGE(mapping);
            }

            static bool coverage[numStates][numEvents];
    #endif // def UNIT_TEST

    private:
        static const Transition transitionTable[numStates][numEvents];
        State state;
    };
    const StateMachine::Transition StateMachine::transitionTable[numStates][numEvents] = {
        {   // idle
            {activationDown, waitMappedDown, discardKey},
            {activationUp, self, 0},
            {mappedKeyDown, self, 0},
            {mappedKeyUp, self, 0},
            {otherKeyDown, self, 0},
            {otherKeyUp, self, 0},
            {configKeyDown, self, 0}
        },
        {   // waitMappedDown
            {activationDown, self, discardKey},
            {activationUp, idle, tapActivationKey},
            {mappedKeyDown, waitMappedUp, delayKeyDown},
            {mappedKeyUp, self, 0},
            {otherKeyDown, waitMappedDownSpaceEmitted, activationKeyDownThenKey}, 
            {otherKeyUp, self, 0},
            {configKeyDown, self, runConfigure} 
        },
        {   // waitMappedDownSpaceEmitted
            {activationDown, self, discardKey},
            {activationUp, idle, 0},
            {mappedKeyDown, waitMappedUpSpaceEmitted, delayKeyDown},
            {mappedKeyUp, self, 0},
            {otherKeyDown, self, 0},
            {otherKeyUp, self, 0},
            {configKeyDown, self, runConfigure} 
        },
        {   // waitMappedUp (still might emit the activation key)
            {activationDown, self, discardKey},
            {activationUp, idle, emitActDownSavedDownActUp},
            {mappedKeyDown, mapping, mapSavedAndMapCurrentDown},
            {mappedKeyUp, mapping, mapSavedAndMapCurrentUp},
            {otherKeyDown, idle, emitActSavedAndCurrentDown},
            {otherKeyUp, waitMappedUpSpaceEmitted, emitActSavedAndCurrentUp},
            {configKeyDown, self, runConfigure} 
        },
        {   // waitMappedUpSpaceEmitted 
            {activationDown, self, discardKey},
            {activationUp, idle, emitSavedDownAndActUp},
            {mappedKeyDown, mapping, mapSavedAndMapCurrentDown},
            {mappedKeyUp, mapping, mapSavedAndMapCurrentUp},
            {otherKeyDown, idle, emitSavedAndCurrentDown},
            {otherKeyUp, self, 0},
            {configKeyDown, self, runConfigure} 
        },
        {   // mapping (definitely eaten the activation key)
            {activationDown, self, discardKey},
            {activationUp, idle, discardKeyAndReleaseMappedKeys},
            {mappedKeyDown, self, mapKeyDown},
            {mappedKeyUp, self, mapKeyUp},
            {otherKeyDown, self, 0},
            {otherKeyUp, self, 0},
            {configKeyDown, self, runConfigure}
        }
    };

    static StateMachine SM;


    LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) { 
        const LRESULT discard = 1; // return value for discarding this event
        hadKeypressSinceLastTick = true;
        if (nCode >= 0) {
            KBDLLHOOKSTRUCT* h = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            logModifierState(h->vkCode, isKeyDown(wParam));

            //wchar_t buff[100];
            //wsprintf(buff, L"%x %x %x %x\n", h->vkCode, h->scanCode, h->flags, h->dwExtraInfo);
            //OutputDebugString(buff);

            if (!injectedByUs(h)) {
                wchar_t exeName[MAX_PATH];
                win32funcs::GetWindowExeName(exeName, MAX_PATH, GetForegroundWindow());

                if (mappingEnabledFor(exeName)) {
                    if (options.trainingMode && !allowedInTrainingMode(h->vkCode) && trainingEnabledFor(exeName)) {
                        if (options.beepForMistakes && isKeyDown(wParam)) MessageBeep(MB_OK);
                        return discard;
                    }
                    // We don't want space then ctrl to emit a space, hence modifier key check
                    if (!win32funcs::IsModifierKey(h->vkCode)) {
                        if (SM.ProcessKey(wParam, h->vkCode)) {
                            return discard;
                        }
                    }
                }
            }
        }

        // normal processing
        return CallNextHookEx(HookHandle, nCode, wParam, lParam); 
    } 

} // namespace


__declspec(dllexport)
void ReHook() {
    if (HookHandle) {
        // Only re-hook if no typing is happening (or our hook function has been blocked),
        // otherwise can get Kyle's stray-letter-insertion bug.
        if (!hadKeypressSinceLastTick) {
            UnhookWindowsHookEx(HookHandle);
            HookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(0), 0);
        }
    }
    hadKeypressSinceLastTick = false;
}


__declspec(dllexport)
bool IsEnabled() {
    return HookHandle != 0;
}

__declspec(dllexport)
void InstallHook() {
    if (!HookHandle && options.enabled) {
        HookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(0), 0); 
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }
}

__declspec(dllexport)
void RemoveHook() {
    if (HookHandle) { 
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        UnhookWindowsHookEx(HookHandle);
        HookHandle = 0;
    }
}

__declspec(dllexport)
void LoadOptions() {
    SM.Reset();
    options.Load();

    if (options.enabled) {
        InstallHook();
    }
    else {
        RemoveHook();
    }
}


__declspec(dllexport)
void Disable() {
    // For end of demo pestering.
    options.enabled = false;
    options.Save();
    RemoveHook();
}


__declspec(dllexport)
bool ShouldShowInNotificationArea() {
    return options.showInNotificationArea;
}

__declspec(dllexport)
void CheckForUpdate() {
    if (options.ShouldCheckForUpdate()) {
        LaunchProgRelative(L"touchcursor_update.exe");
    }
}

//---------------------------------------------------------------------------
#ifdef UNIT_TEST
//---------------------------------------------------------------------------
bool StateMachine::coverage[numStates][numEvents];


#include <stdarg.h>

namespace test {

    // assumes default key mapping
    const DWORD dn = 0;
    const DWORD edn = KEYEVENTF_EXTENDEDKEY;
    const DWORD up = LLKHF_UP;
    const DWORD j = L'J';
    const DWORD m = L'M';
    const DWORD x = L'X';
    const DWORD c = L'C';
    const DWORD SP = VK_SPACE;
    const DWORD LE = VK_LEFT;
    const DWORD DEL = VK_DELETE;
    const DWORD F5 = configKey;
    const DWORD ctrl = VK_CONTROL;

    int failures = 0;

    #define WIDEN2(x) L ## x
    #define WIDEN(x) WIDEN2(x)
    #define __WFILE__ WIDEN(__FILE__)
    const wchar_t *file = __WFILE__;
    unsigned line;

    void check(DWORD key, DWORD flag, DWORD expected, ...) {
        
        KBDLLHOOKSTRUCT h = {key, 0, flag, 0, 0};
        LowLevelKeyboardProc(HC_ACTION, flag==dn ? WM_KEYDOWN : WM_KEYUP, reinterpret_cast<LPARAM>(&h));

        DWORD e = expected;
        va_list marker;
        va_start(marker, expected);
        DWORD* actual = keyOutput;
        wchar_t buff[1000];

        while (e) {
            for (int flag=0; flag<2; ++flag) {
                bool match = false;
                if (flag) match = (e == up) ? (*actual & (e | KEYEVENTF_KEYUP)) != 0 : e == *actual;
                else match = e == *actual;

                if (!match) {

                    if (flag) {                        
                        wsprintf(buff, L"%s (%i) : Flag mismatch: expected 0x%x, got 0x%x\n", file, line, e, *actual);
                    }
                    else {
                        wsprintf(buff, L"%s (%i) : Key mismatch: Expected 0x%x ('%c'), got 0x%x ('%c')\n", file, line, e, e, *actual, *actual);
                    }
                    OutputDebugString(buff);
                    ++failures;
                }
                ++actual;
                e = va_arg(marker, DWORD);
            }
        }
        if (*actual) {
            wsprintf(buff, L"%s (%i) : Unexpected extra output: 0x%x ('%c')\n", file, line, *actual, *actual);            
            OutputDebugString(buff);
            ++failures;
        }
        va_end(marker);
    }

    #define CHECK(ARGS) {line = __LINE__; check ARGS;}

    struct Tester {
        Tester() {
            isInUnitTest = true;
            Options oldOptions = options;
            options = Options(Options::defaults);

            assert(isExtendedKey(VK_LEFT));
            assert(isExtendedKey(VK_RMENU));
            assert(!isExtendedKey(VK_F1));
            assert(!isExtendedKey(0));

            // normal (slow) typing
            resetOutput();
            CHECK((SP, up,  SP,up, 0));
            CHECK((SP, dn,  SP,up, 0));
            CHECK((SP, up,  SP,up, SP,dn, SP,up, 0));
            CHECK((x, dn,   SP,up, SP,dn, SP,up, x,dn, 0));
            CHECK((x, up,   SP,up, SP,dn, SP,up, x,dn, x,up, 0));
            CHECK((j, dn,   SP,up, SP,dn, SP,up, x,dn, x,up, j,dn, 0));
            CHECK((j, up,   SP,up, SP,dn, SP,up, x,dn, x,up, j,dn, j,up, 0));

            // overlapped slightly
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((x, dn,   SP,dn, x,dn, 0));
            CHECK((SP, up,  SP,dn, x,dn, SP,up, 0));
            CHECK((x, up,   SP,dn, x,dn, SP,up, x,up, 0));

            //... plus repeating spaces
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((SP, dn,  0));
            CHECK((j, dn,   0));
            CHECK((SP, dn,  0));
            CHECK((SP, up,  SP,dn, j,dn, SP,up, 0));
            CHECK((j, up,   SP,dn, j,dn, SP,up, j,up, 0));

            // key ups in waitMappedDown
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((x, up,   x,up, 0));
            CHECK((j, up,   x,up, j,up, 0));
            CHECK((SP, up,  x,up, j,up, SP,dn, SP,up, 0));

            // other keys in waitMappedUp
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((j, dn,   0));
            CHECK((x, up,   SP,dn, j,dn, x,up, 0));
            CHECK((x, dn,   SP,dn, j,dn, x,up, x,dn, 0));
            CHECK((j, dn,   SP,dn, j,dn, x,up, x,dn, j,dn, 0));
            CHECK((SP, up,  SP,dn, j,dn, x,up, x,dn, j,dn, SP,up, 0));

            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((j, dn,   0));
            CHECK((x, dn,   SP,dn, j,dn, x,dn, 0));
            CHECK((SP, up,  SP,dn, j,dn, x,dn, SP,up, 0));


            // activate mapping
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((j, dn,   0));
            CHECK((j, up,   LE,edn, LE,up, 0));
            CHECK((SP, up,  LE,edn, LE,up, 0));

            // autorepeat into mapping, and out
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((j, dn,   0));
            CHECK((j, dn,   LE,edn, LE,edn, 0));
            CHECK((j, dn,   LE,edn, LE,edn, LE,edn, 0));
            CHECK((j, up,   LE,edn, LE,edn, LE,edn, LE,up, 0));
            CHECK((SP, dn,  LE,edn, LE,edn, LE,edn, LE,up, 0));
            CHECK((j, dn,   LE,edn, LE,edn, LE,edn, LE,up, LE,edn, 0));
            CHECK((SP, up,  LE,edn, LE,edn, LE,edn, LE,up, LE,edn, LE,up, 0));
            CHECK((j, dn,   LE,edn, LE,edn, LE,edn, LE,up, LE,edn, LE,up, j,dn, 0));
            CHECK((j, up,   LE,edn, LE,edn, LE,edn, LE,up, LE,edn, LE,up, j,dn, j,up, 0));

            // other keys during mapping
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((j, dn,   0));
            CHECK((j, up,   LE,edn, LE,up, 0));
            CHECK((x, dn,   LE,edn, LE,up, x,dn, 0));
            CHECK((x, up,   LE,edn, LE,up, x,dn, x,up, 0));
            CHECK((j, dn,   LE,edn, LE,up, x,dn, x,up, LE,edn, 0));
            CHECK((SP, up,  LE,edn, LE,up, x,dn, x,up, LE,edn, LE,up, 0));

            // check space-emmitted states
            // wmdse
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((x, dn,   SP,dn, x,dn, 0));
            CHECK((SP, dn,  SP,dn, x,dn, 0));
            CHECK((x, dn,   SP,dn, x,dn, x,dn, 0));
            CHECK((x, up,   SP,dn, x,dn, x,dn, x,up, 0));
            CHECK((j, up,   SP,dn, x,dn, x,dn, x,up, j,up, 0));
            CHECK((j, dn,   SP,dn, x,dn, x,dn, x,up, j,up, 0));
            CHECK((j, up,   SP,dn, x,dn, x,dn, x,up, j,up, LE,edn, LE,up, 0));
            CHECK((SP, up,  SP,dn, x,dn, x,dn, x,up, j,up, LE,edn, LE,up, 0)); //XXX should this emit a space (needs mappingSpaceEmitted state)

            // wmuse
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((x, dn,   SP,dn, x,dn, 0));
            CHECK((j, dn,   SP,dn, x,dn, 0));
            CHECK((SP, dn,  SP,dn, x,dn, 0));
            CHECK((SP, up,  SP,dn, x,dn, j,dn, SP,up, 0));

            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((x, dn,   SP,dn, x,dn, 0));
            CHECK((j, dn,   SP,dn, x,dn, 0));
            CHECK((j, dn,   SP,dn, x,dn, LE,edn, LE,edn, 0));
            CHECK((SP, up,  SP,dn, x,dn, LE,edn, LE,edn, LE,up, 0)); //XXX should this emit a space (needs mappingSpaceEmitted state)

            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((x, dn,   SP,dn, x,dn, 0));
            CHECK((j, dn,   SP,dn, x,dn, 0));
            CHECK((x, up,   SP,dn, x,dn, x,up, 0));
            CHECK((j, up,   SP,dn, x,dn, x,up, LE,edn, LE,up, 0));
            CHECK((SP, up,  SP,dn, x,dn, x,up, LE,edn, LE,up, 0)); //XXX should this emit a space (needs mappingSpaceEmitted state)
            
            // run configure tests
            resetOutput();
            //idle
            CHECK((F5, dn,  F5,dn, 0)); 
            CHECK((SP, dn,  F5,dn, 0)); 
            // wmd
            CHECK((F5, dn,  F5,dn, '*',dn, 0)); 
            CHECK((F5, up,  F5,dn, '*',dn, F5,up, 0));
            CHECK((j, dn,   F5,dn, '*',dn, F5,up, 0));
            // wmu
            CHECK((F5, dn,  F5,dn, '*',dn, F5,up, '*',dn, 0));
            CHECK((j, up,   F5,dn, '*',dn, F5,up, '*',dn, LE,edn, LE,up, 0));
            // mapping
            CHECK((F5, dn,  F5,dn, '*',dn, F5,up, '*',dn, LE,edn, LE,up, '*',dn, 0));
            CHECK((SP, up,  F5,dn, '*',dn, F5,up, '*',dn, LE,edn, LE,up, '*',dn, 0));

            resetOutput();
            CHECK((SP, dn,  0)); 
            // wmd
            CHECK((x, dn,   SP,dn, x,dn, 0));
            // wmd-se
            CHECK((F5, dn,  SP,dn, x,dn, '*',dn, 0));
            CHECK((j, dn,   SP,dn, x,dn, '*',dn, 0));
            // wmu-se
            CHECK((F5, dn,  SP,dn, x,dn, '*',dn, '*',dn, 0));
            CHECK((SP, up,  SP,dn, x,dn, '*',dn, '*',dn, j,dn, SP,up, 0));

            // Overlapping mapped keys
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((m, dn,   0));
            CHECK((j, dn,   DEL,edn, LE,edn, 0));
            CHECK((j, up,   DEL,edn, LE,edn, LE,up, 0));
            CHECK((m, up,   DEL,edn, LE,edn, LE,up, DEL,up, 0));
            CHECK((SP, up,  DEL,edn, LE,edn, LE,up, DEL,up, 0));

            // Overlapping mapped keys -- space up first.
            // should release held mapped keys.  (Fixes sticky Shift bug.)
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((m, dn,   0));
            CHECK((j, dn,   DEL,edn, LE,edn, 0));
            // release order is in vk code order
            CHECK((SP, up,  DEL,edn, LE,edn, LE,up, DEL,up, 0));

            // mapped modifier keys
            options.keyMapping['C'] = ctrlFlag | 'C'; // ctrl+c
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((c, dn,   0));
            CHECK((c, up,   ctrl,dn, c,dn, ctrl,up, c,up, 0));
            CHECK((c, dn,   ctrl,dn, c,dn, ctrl,up, c,up, ctrl,dn, c,dn, ctrl,up, 0));
            CHECK((c, up,   ctrl,dn, c,dn, ctrl,up, c,up, ctrl,dn, c,dn, ctrl,up, c,up, 0));
            CHECK((SP, up,  ctrl,dn, c,dn, ctrl,up, c,up, ctrl,dn, c,dn, ctrl,up, c,up, 0));
            // with modifier already down:
            resetOutput();
            CHECK((SP, dn,  0));
            CHECK((ctrl,dn, ctrl,dn, 0));
            CHECK((c, dn,   ctrl,dn, 0));
            CHECK((c, up,   ctrl,dn, c,dn, c,up, 0));
            CHECK((ctrl,up, ctrl,dn, c,dn, c,up, ctrl,up, 0));
            CHECK((SP,up,   ctrl,dn, c,dn, c,up, ctrl,up, 0));

            // training mode
            options.trainingMode = true;
            options.beepForMistakes = false;
            resetOutput();
            CHECK((x, dn,   x,dn, 0));
            CHECK((x, up,   x,dn, x,up, 0));
            CHECK((LE, edn, x,dn, x,up, 0));
            CHECK((LE, up,  x,dn, x,up, 0));
            // with modifier mapping
            resetOutput();
            CHECK((c, dn,    c,dn, 0));
            CHECK((c, up,    c,dn, c,up, 0));
            CHECK((ctrl, dn, c,dn, c,up, ctrl,dn, 0));
            CHECK((c, dn,    c,dn, c,up, ctrl,dn, 0));
            CHECK((c, up,    c,dn, c,up, ctrl,dn, 0));
            CHECK((ctrl, up, c,dn, c,up, ctrl,dn, ctrl,up, 0));

            SM.printUnusedTransitions();
            if (failures) exit(failures);
            else OutputDebugString(L"Unit tests passed\n");

            options = oldOptions;
            isInUnitTest = false;
        }
    };

    Tester tester;
}

//---------------------------------------------------------------------------
#endif //def UNIT_TEST
//---------------------------------------------------------------------------
