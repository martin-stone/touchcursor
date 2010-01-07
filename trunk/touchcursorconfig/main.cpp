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


#pragma warning(disable: 4267)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list

#include "main.h"

#include "options.h"
#include "win32funcs.h"
#include "launch.h"
#include "versionupdate.h"

#include <wx/wx.h>
#include <wx/xrc/xmlres.h>
#include <wx/xrc/xh_all.h>
#include <wx/event.h>
#include <wx/msw/registry.h>
#include <wx/html/htmlwin.h>

#include "xrcres.h"

#include "boost/scoped_ptr.hpp"
#include "boost/algorithm/string/split.hpp"
#include "boost/algorithm/string/trim.hpp"
#include "boost/bind.hpp"
#include "boost/array.hpp"
#include <algorithm>

//---------------------------------------------------------------------------

namespace {
    void updateRegistryRunKey(bool runAtStartup) {
        const wxChar* const tcRegName = wxT("TouchCursor");
        wxRegKey key(wxT("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"));
        if(!key.Exists()) key.Create(); // Run key doesn't exist on a pristine install (Win 2k anyway)

        if (runAtStartup) {
            wxChar folder[MAX_PATH];
            GetProgramFolder(folder);
            key.SetValue(tcRegName, wxT("\"") + wxString(folder) + wxT("\\touchcursor.exe") + wxT("\""));
        }
        else {
            key.DeleteValue(tcRegName);
        }
    }
}

//---------------------------------------------------------------------------

class ProgList : wxEvtHandler {
public:
    ProgList(const wxChar* label, wxWindow* parent, wxSizer* sizer, bool startGroup) 
    {   
        radioButton = new wxRadioButton(parent, -1, label, wxDefaultPosition, wxDefaultSize, 
            startGroup ? wxRB_GROUP : 0);
        sizer->Add(radioButton);

        editBox = new wxTextCtrl(parent, -1);
        int h = editBox->GetSize().GetHeight();
        editButton = new wxButton(parent, -1, wxT("..."), wxDefaultPosition, wxSize(h, h));
        wxFlexGridSizer* editSizer = new wxFlexGridSizer(2);
        editSizer->AddGrowableCol(0);
        editSizer->Add(editBox, 0, wxEXPAND|wxALL);
        editSizer->Add(editButton);
        sizer->Add(editSizer, 0, wxEXPAND|wxALL);

        Connect(editButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ProgList::onEditButton));
        parent->PushEventHandler(this);
    }

    ~ProgList() {
        radioButton->GetParent()->PopEventHandler();
    }

    void UpdateUi() {
        bool enable = radioButton->GetValue();
        editBox->Enable(enable);
        editButton->Enable(enable);
    }

    void SetValue(const Options::StringVec& progList) {
        wxString boxValue;
        for (Options::StringVec::const_iterator i=progList.begin(); i!=progList.end(); ++i) {
            if (i != progList.begin()) boxValue += wxT("; ");
            boxValue += i->c_str();
        }
        editBox->SetValue(boxValue);
    }

    static bool isSep(wxChar c) {
        return c == ';';
    }

    Options::StringVec GetValue() const {
        // split ';' separated strings and trim
        Options::StringVec result;
        std::wstring value = editBox->GetValue().c_str();
        boost::algorithm::split(result, value, isSep, boost::algorithm::token_compress_on); 
        std::for_each(result.begin(), result.end(), boost::bind(boost::algorithm::trim<std::wstring>, _1, std::locale()));
        result.erase(std::remove_if(result.begin(), result.end(), std::mem_fun_ref(&std::wstring::empty)), result.end());
        return result;
    }

    void Enable(bool enable) {
        radioButton->SetValue(enable);
    }

    bool IsEnabled() const {
        return radioButton->GetValue();
    }

private:
    void onEditButton(wxCommandEvent& event) {
        wxString programsInOut = editBox->GetValue();
        bool updated = EditProgList(programsInOut, editButton);
        if (updated) {
            editBox->SetValue(programsInOut);
        }
    }

private:
    wxRadioButton* radioButton;
    wxTextCtrl* editBox;
    wxButton* editButton;
};

//---------------------------------------------------------------------------

class MainFrame : public MainFrameBase {
public:
    MainFrame() {
        SetBackgroundColour(GetChildren()[0]->GetBackgroundColour());
        createProgramsPage();
        populateActivationKeyList();
        sendOptionsToControls(options);
        KeyList->SetFont(wxSystemSettings::GetFont(wxSYS_ANSI_FIXED_FONT));

        wxAcceleratorEntry entries[2];
        entries[0].Set(wxACCEL_NORMAL, WXK_ESCAPE, CancelButton->GetId());
        entries[1].Set(wxACCEL_NORMAL, WXK_F1, HelpButton->GetId());
        wxAcceleratorTable accel(2, entries);
        SetAcceleratorTable(accel);
    };

    void createProgramsPage() {
        wxSizer* sizer = Programs->GetSizer()->GetItem(size_t(0))->GetSizer();
        assert(sizer);
        if (sizer) {
            sizer->Add(new wxStaticText(Programs, -1, wxT("Cursor Mode")));
            disableProgs.reset(new ProgList(wxT("&Disable with the following programs"), Programs, sizer, true));
            enableProgs.reset(new ProgList(wxT("&Enable only with the following programs"), Programs, sizer, false));
            sizer->AddSpacer(8);
            sizer->Add(new wxStaticText(Programs, -1, wxT("Training Mode")));
            neverTrainProgs.reset(new ProgList(wxT("&Never train in the following programs"), Programs, sizer, true));
            onlyTrainProgs.reset(new ProgList(wxT("&Only train in the following programs"), Programs, sizer, false));
        }
    }

    void populateActivationKeyList() {
        ActivationKeys->Clear();
        //const int activationOptions[] = {VK_SPACE, VK_CAPITAL, VK_TAB, VK_OEM_1, VK_OEM_3, VK_OEM_7, 0}; //XXX win-specific codes in otherwise x-platform file :-(
        //for (const int* p = activationOptions; *p; ++p) {
        //    ActivationKeys->Append(win32funcs::VkCodeToStr(*p), reinterpret_cast<void*>(*p));
        //}
        for (size_t i=1; i<0x100; ++i) {
            if (!win32funcs::IsModifierKey(i)) {
                wxString name = win32funcs::VkCodeToStr(i);
                if (!name.empty()) {
                    ActivationKeys->Append(name, reinterpret_cast<void*>(i));
                }
            }
        }
    }

    int activationKeyCode(int index) const {
        return reinterpret_cast<size_t>(ActivationKeys->GetClientData(index));
    }

    int selectedActivationKey() const {
        return activationKeyCode(ActivationKeys->GetSelection());
    }

    Options readOptionsFromControls() {
        Options tempOpts = options; // Do NOT use defaults, as that resets the first run day

        tempOpts.enabled = Enabled->GetValue();
        tempOpts.trainingMode = Training->GetValue();
        tempOpts.beepForMistakes = BeepForMistakes->GetValue();
        tempOpts.runAtStartup = RunAtStartup->GetValue();
        tempOpts.showInNotificationArea = ShowInNotificationArea->GetValue();
        tempOpts.checkForUpdates = CheckForUpdates->GetValue();

        tempOpts.activationKey = selectedActivationKey();

        MappingFromBox(tempOpts.keyMapping, tempOpts.maxCodes, KeyList);

        tempOpts.disableProgs = disableProgs->GetValue();
        tempOpts.enableProgs = enableProgs->GetValue();
        tempOpts.neverTrainProgs = neverTrainProgs->GetValue();
        tempOpts.onlyTrainProgs = onlyTrainProgs->GetValue();

        tempOpts.useEnableList = enableProgs->IsEnabled();
        tempOpts.useOnlyTrainList = onlyTrainProgs->IsEnabled();

        return tempOpts;
    }

    void sendOptionsToControls(const Options& options) {
        Enabled->SetValue(options.enabled);
        Training->SetValue(options.trainingMode);
        BeepForMistakes->SetValue(options.beepForMistakes);
        RunAtStartup->SetValue(options.runAtStartup);
        ShowInNotificationArea->SetValue(options.showInNotificationArea);
        CheckForUpdates->SetValue(options.checkForUpdates);

        // look up activation key option
        for (unsigned i=0; i<ActivationKeys->GetCount(); ++i) {
            if (activationKeyCode(i) == options.activationKey) {
                ActivationKeys->SetSelection(i);
                break;
            }
         }

        MappingToBox(options.keyMapping, options.maxCodes, KeyList);

        disableProgs->SetValue(options.disableProgs);
        enableProgs->SetValue(options.enableProgs);
        neverTrainProgs->SetValue(options.neverTrainProgs);
        onlyTrainProgs->SetValue(options.onlyTrainProgs);

        enableProgs->Enable(options.useEnableList);
        onlyTrainProgs->Enable(options.useOnlyTrainList);
    }

    void applyChanges() {
        options = readOptionsFromControls();
        updateRegistryRunKey(options.runAtStartup);
        options.Save();
        LaunchProgRelative(wxT("touchcursor.exe"));
    }

    void onOkButton(wxCommandEvent& event) {
        applyChanges();
        Close();
    }

    void onCancelButton(wxCommandEvent& event) {
        Close();
    }

    void onApplyButton(wxCommandEvent& event) {
        applyChanges();
        CancelButton->SetLabel(wxT("Close"));
    }

    bool optionsAreDirty() {
        Options newOptions = readOptionsFromControls();
        return newOptions.AsString() != options.AsString(); 
    }

    void onIdle(wxIdleEvent& event) {
        ApplyButton->Enable(optionsAreDirty());
        BeepForMistakes->Enable(Training->GetValue());
        disableProgs->UpdateUi();
        enableProgs->UpdateUi();
        neverTrainProgs->UpdateUi();
        onlyTrainProgs->UpdateUi();
    }

    // K e y s   t a b

    void onDefaultsButton(wxCommandEvent& event) {
        int answer = wxMessageBox(wxT("Replace current key bindings with defaults?"), wxT("Restore Defaults?"), wxYES_NO | wxICON_QUESTION, this);
        if (answer == wxYES) {
            Options tempOpts(Options::defaults);
            MappingToBox(tempOpts.keyMapping, tempOpts.maxCodes, KeyList);
        }
    }

    void onAddBindingButton(wxCommandEvent& event) {
        AddBinding(KeyList, selectedActivationKey());
        onKeyListSelChange(event);
    }

    void onEditBindingButton(wxCommandEvent&) {
        EditBinding(KeyList, KeyList->GetSelection(), selectedActivationKey());
    }

    void onEditBinding(wxCommandEvent& event) {
        EditBinding(KeyList, event.GetInt(), selectedActivationKey());
    } 

    void onRemoveBindingButton(wxCommandEvent& event) {
        int item = KeyList->GetSelection();
        if (item != wxNOT_FOUND) {
            KeyList->Delete(item);
            onKeyListSelChange(event);
        }
    }

    void onKeyListSelChange(wxCommandEvent&) {
        RemoveBindingButton->Enable(KeyList->GetSelection() != wxNOT_FOUND);
        EditBindingButton->Enable(KeyList->GetSelection() != wxNOT_FOUND);
    }

    // A b o u t   T a b

    void onLicenseHyperlink(wxHtmlLinkEvent& e) {
        LaunchUrl(e.GetLinkInfo().GetHref());
    }

    void onHelpButton(wxCommandEvent&) {
        LaunchLocalHtml(wxT("docs\\help.html"));
    }

    void onWebSiteButton(wxCommandEvent&) {
        LaunchUrl(wxT("http://touchcursor.sourceforge.net/"));
    }

    void onDonateButton(wxCommandEvent&) {
        LaunchUrl(wxT("http://touchcursor.sourceforge.net/")); // Button is disabled for now.
    }

    void onCheckForUpdateButton(wxCommandEvent&) {
        wxBusyCursor hourglass;
        tclib::VersionStatus status = tclib::GetVersionStatus();
        switch (status) {
            case tclib::dontKnow:
                wxMessageBox(wxT("Could not connect to the TouchCursor web site.  Please try again later."), wxT("Version Check"), wxOK | wxICON_ERROR, this);
                break;

            case tclib::upToDate:
                wxMessageBox(wxT("You are running the latest version."), wxT("Version Check"), wxOK | wxICON_INFORMATION, this);
                break;

            case tclib::outOfDate: {
                int answer = wxMessageBox(wxT("A new version is available.  Download it now?\n\n(Clicking 'Yes' will open your browser at the TouchCursor web site.)"), 
                    wxT("Version Check"), wxYES_NO | wxICON_EXCLAMATION, this);
                if (answer == wxYES) {
                    tclib::BrowseToDownload();
                }
                break;
            }                                  
        }
    }


private:
    Options options;
    boost::scoped_ptr<ProgList> disableProgs;
    boost::scoped_ptr<ProgList> enableProgs;
    boost::scoped_ptr<ProgList> neverTrainProgs;
    boost::scoped_ptr<ProgList> onlyTrainProgs;
    std::wstring pendingRegKey;
    DECLARE_EVENT_TABLE();
};

#define MAP_BUTTON(BUTTONID) \
    EVT_BUTTON(XRCID(#BUTTONID), MainFrame::on##BUTTONID)

BEGIN_EVENT_TABLE(MainFrame, MainFrameBase)
MAP_BUTTON(OkButton)
MAP_BUTTON(CancelButton)
MAP_BUTTON(ApplyButton)
EVT_IDLE(MainFrame::onIdle)
// General tab
MAP_BUTTON(DefaultsButton)
MAP_BUTTON(AddBindingButton)
MAP_BUTTON(EditBindingButton)
MAP_BUTTON(RemoveBindingButton)
EVT_LISTBOX(XRCID("KeyList"), MainFrame::onKeyListSelChange)
EVT_LISTBOX_DCLICK(XRCID("KeyList"), MainFrame::onEditBinding)
// About
MAP_BUTTON(HelpButton)
MAP_BUTTON(WebSiteButton)
MAP_BUTTON(CheckForUpdateButton)
MAP_BUTTON(DonateButton)
EVT_HTML_LINK_CLICKED(XRCID("LicenseText"), MainFrame::onLicenseHyperlink)
END_EVENT_TABLE()

//---------------------------------------------------------------------------

class TcConfigApp : public wxApp {
private:

    virtual bool OnInit() {
        // Activate an earlier instance if found,
        // or quit it if argv[1] == wxT("quit")
        bool quit = argc>1 && !wxStricmp(argv[1], wxT("quit"));
        const wxChar* title = wxT("TouchCursor Configuration");
        if (quit) {
            win32funcs::QuitExistingWindow(title);
            return false;
        }
        else {
            bool found = win32funcs::RaiseExistingWindow(title);
            if (found) return false;
        }

        wxXmlResource::Get()->AddHandler(new wxFrameXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxDialogXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxNotebookXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxPanelXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxCheckBoxXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxRadioButtonXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxChoiceXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxListBoxXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxButtonXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxStaticTextXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxTextCtrlXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxSizerXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxStaticBitmapXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxStaticLineXmlHandler);
        wxXmlResource::Get()->AddHandler(new wxHtmlWindowXmlHandler);

        InitXmlResource();

        MainFrame* frame = new MainFrame;
        frame->SetIcon(wxICON(APP));
        assert(frame->GetTitle() == title);
        frame->Show();
        SetTopWindow(frame);
        return true;
    }
};

IMPLEMENT_APP(TcConfigApp)
