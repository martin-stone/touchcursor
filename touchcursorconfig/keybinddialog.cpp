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


#include "modifiers.h"
#include "main.h"
#include "win32funcs.h"

#include <algorithm>
#include <wx/xrc/xmlres.h>
#include <wx/html/htmlwin.h>
#include "xrcres.h"


namespace {

    wxString codeToKeyStr(int winCode) {
        return win32funcs::VkCodeToStr(winCode & 0xff);
    }

    wxString codeToFullStr(int winCode) {
        wxString modifiers;
        if (shiftFlag & winCode) modifiers += L"Shift + ";
        if (ctrlFlag & winCode) modifiers += L"Ctrl + ";
        if (altFlag & winCode) modifiers += L"Alt + ";
        if (winFlag & winCode) modifiers += L"Win + ";
        return modifiers + codeToKeyStr(winCode);
    }


    class BindingDialog : public BindingDialogBase {

        struct KeyTranslator : wxEvtHandler {
            KeyTranslator(wxTextCtrl* textCtrl, int code, wxWindow* windowToFocus=0, int winCodeToBlock=0) 
                : textCtrl(textCtrl) 
                , code(code & 0xff)
                , windowToFocus(windowToFocus)
                , winCodeToBlock(winCodeToBlock)
            {
                textCtrl->SetValue(codeToKeyStr(code));
                textCtrl->PushEventHandler(this);
                Connect(textCtrl->GetId(), wxEVT_KEY_DOWN, wxKeyEventHandler(KeyTranslator::onKey));
            }

            ~KeyTranslator() {
                textCtrl->PopEventHandler();
            }

            bool setKey(int winCode) {
                wxString s = codeToKeyStr(winCode);
                if (!s.empty()) {
                    textCtrl->SetValue(s);
                    code = winCode;
                    return true;
                }
                else {
                    return false;
                }
            }

            bool shouldProcess(int winCode) const {
                // If we're not blocking the activation key, we don't need to block modifier keys either.
                return winCodeToBlock == 0
                    || (winCode != winCodeToBlock && !win32funcs::IsModifierKey(winCode)); 
            }

            void onKey(wxKeyEvent& event) {
                if (event.AltDown()) {
                    event.Skip();
                }
                else {
                    int winCode = event.GetRawKeyCode();
                    winCode = win32funcs::DiscriminateLeftRightModifierJustPressed(winCode);

                    if (shouldProcess(winCode))  {
                        bool ok = setKey(winCode);
                        if (ok && windowToFocus) windowToFocus->SetFocusFromKbd();
                        //::wxLogDebug(wxT("%i, %x\n"), event.GetId(), winCode);
                    }
                }
            }

            wxTextCtrl* textCtrl;
            int code;
            wxWindow* windowToFocus;
            int winCodeToBlock;
        };

    public:
        BindingDialog(wxWindow* parent, int from, int to, int activationKeyWinCode, bool newEntry)
            : BindingDialogBase(parent)
            , fromTranslator(KeyToPress, from, KeyToGenerate, activationKeyWinCode)
            , toTranslator(KeyToGenerate, to)
        {
            int h = KeyToGenerate->GetSize().GetHeight();
            GetSizer()->SetItemMinSize(KeyToPressButton, h, h);
            GetSizer()->SetItemMinSize(KeyToGenerateButton, h, h);
            Connect(KeyToPressButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(BindingDialog::onChooseKey));
            Connect(KeyToGenerateButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(BindingDialog::onChooseKey));
            ActivationKeyLabel->SetLabel(codeToKeyStr(activationKeyWinCode) + wxT(" + "));
            setModifiers(to);
            if (!newEntry) KeyToGenerate->SetFocus();
        }

        void onChooseKey(wxCommandEvent& event) {

            struct KeyDialog : KeyDialogBase {
                KeyDialog(wxWindow* parent, const KeyTranslator& translator) 
                    : KeyDialogBase(parent) 
                {
                    Connect(KeyList->GetId(), wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, wxCommandEventHandler(KeyDialog::onDblClick));

                    for (int i=1; i<0x100; ++i) {
                        if (translator.shouldProcess(i)) {
                            wxString name = codeToKeyStr(i);
                            if (!name.empty()) {
                                int pos = KeyList->Append(name, reinterpret_cast<void*>(size_t(i)));
                                if (i == translator.code) {
                                    KeyList->SetSelection(pos);
                                }
                            }
                        }
                    }
                    Fit();
                    Centre();
                }

                int getSelectedVk() {
                    return static_cast<int>(reinterpret_cast<size_t>(KeyList->GetClientData(KeyList->GetSelection())));
                }

                void onDblClick(wxCommandEvent&) {
                    EndModal(::wxID_OK);
                }
            };

            KeyTranslator& trans = event.GetId() == KeyToPressButton->GetId() ? fromTranslator : toTranslator;
            KeyDialog dialog(this, trans);

            if (dialog.ShowModal() == ::wxID_OK) {
                trans.setKey(dialog.getSelectedVk());
            }
        }

        int getFrom() const {
            return fromTranslator.code;
        }

        int getTo() const {
            return toTranslator.code | getModifiers();
        }

    private:

        int getModifiers() const {
            int modifiers = 0;
            if (Shift->GetValue()) modifiers |= shiftFlag;
            if (Ctrl->GetValue()) modifiers |= ctrlFlag;
            if (Alt->GetValue()) modifiers |= altFlag;
            if (Win->GetValue()) modifiers |= winFlag;
            return modifiers;
        }

        void setModifiers(int to) {
            Shift->SetValue(shiftFlag & to);
            Ctrl->SetValue(ctrlFlag & to);
            Alt->SetValue(altFlag & to);
            Win->SetValue(winFlag & to);
        }

    private:
        KeyTranslator fromTranslator;
        KeyTranslator toTranslator;
    };


    struct Binding : wxClientData {
        Binding(int from, int to) : from(from), to(to) {}
        int from;
        int to;
    };


    const Binding* bindingAtIndex(wxListBox* keyList, int i) {
        if (i == wxNOT_FOUND) return 0;
        else return static_cast<Binding*>(keyList->GetClientObject(i));
    }

    int getListIndex(int from, wxListBox* keyList) {
        for (unsigned i=0; i<keyList->GetCount(); ++i) {
            const Binding* b = bindingAtIndex(keyList, i);
            if (b && b->from == from) return i;
        }
        return wxNOT_FOUND;
    }


    int setBinding(wxListBox* keyList, int from, int to) {
        wxString s = codeToKeyStr(from) + wxT(" -- ") + codeToFullStr(to);
        int i = getListIndex(from, keyList);
        if (i == wxNOT_FOUND) {
            if (from && to) { // XXX more user interaction for blanks?
                i = keyList->Append(s, new Binding(from, to));
            }
        }
        else {
            keyList->SetClientObject(i, new Binding(from, to));
            // must be second because of sorting:
            keyList->SetString(i, s);
            // index may have changed: update
            i = keyList->FindString(s);
        }
        return i;
    }


    int getBinding(int from, wxListBox* keyList) {
        int i = getListIndex(from, keyList);
        if (i != wxNOT_FOUND) {
            const Binding* b = bindingAtIndex(keyList, i);
            if (b && b->from == from) {
                return b->to;
            }
        }
        return 0;
    }
}


void EditBinding(wxListBox* keyList, int selection, int activationKeyWinCode) {
    const Binding* b = bindingAtIndex(keyList, selection);
    bool newEntry = b == 0;

    const Binding none(0, 0);
    b = newEntry ? &none : b;

    BindingDialog dlg(keyList, b->from, b->to, activationKeyWinCode, newEntry);

    int answer = wxNO;
    while (answer == wxNO && dlg.ShowModal() == wxID_OK) {
        int from = dlg.getFrom();
        int to = dlg.getTo();

        if (newEntry) {
            // Check for existing binding
            int oldTo = getBinding(from, keyList);
            if (oldTo && oldTo != to) {
                 answer = wxMessageBox(wxT("Key '") + codeToKeyStr(from) + wxT("' currently generates '") + codeToFullStr(oldTo) 
                    + wxT("'\nDo you want to replace this binding?"), wxT("Replace binding?"), wxYES_NO | wxICON_QUESTION, keyList);
                if (answer == wxNO) continue;
            }
        }
        answer = wxYES;

        if (b) {
            int oldIdx = getListIndex(b->from, keyList);
            if (oldIdx != wxNOT_FOUND) keyList->Delete(oldIdx);
        }
        int newIndex = setBinding(keyList, from, to);
        keyList->SetSelection(newIndex);
    }
}


void AddBinding(wxListBox* keyList, int activationKeyWinCode) {
    EditBinding(keyList, wxNOT_FOUND, activationKeyWinCode);
}


void MappingFromBox(int* keyMapping, int mappingLen, wxListBox* keyList) {
    std::fill(keyMapping, keyMapping + mappingLen, 0);
    for (unsigned i=0; i<keyList->GetCount(); ++i) {
        const Binding* b = bindingAtIndex(keyList, i);
        if (b && 0 < b->from && b->from < mappingLen) {
            keyMapping[b->from] = b->to;
        }
    }
}


void MappingToBox(const int* keyMapping, int mappingLen, wxListBox* keyList) {
    keyList->Clear();
    for (int i=0; i<mappingLen; ++i) {
        if (keyMapping[i]) {
            setBinding(keyList, i, keyMapping[i]);
        }
    }
}

