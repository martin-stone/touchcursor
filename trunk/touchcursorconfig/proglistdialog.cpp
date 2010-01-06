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


#pragma warning(disable: 4355) // 'this' : used in base member initializer list

#include "main.h"
#include "win32funcs.h"

#include <wx/tokenzr.h>

#include <wx/xrc/xmlres.h>
#include <wx/html/htmlwin.h>
#include "xrcres.h"


namespace {

    class ProgListDialog : public ProgListDialogBase {    
    public:
        ProgListDialog(wxWindow* parent, const wxString currentList)
            : ProgListDialogBase(parent)
            , dragHandler(*this) 
        {
            wxStringTokenizer tokens(currentList, wxT(";"), wxTOKEN_STRTOK);
            while (tokens.HasMoreTokens()) {
                wxString prog = tokens.GetNextToken().Strip(wxString::both);
                if (!prog.empty()) ProgList->Append(prog);
            }

            Connect(RemoveButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ProgListDialog::onRemoveButton));
            Connect(ProgList->GetId(), wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(ProgListDialog::onListSelChange));
            Connect(-1, wxEVT_ACTIVATE, wxActivateEventHandler(ProgListDialog::onActivate));
            DragIcon->PushEventHandler(&dragHandler);
            RemoveButton->Disable();
        }

        ~ProgListDialog() {
            DragIcon->PopEventHandler();
        }

        wxString GetPrograms() const {
            wxString listString;
            for (unsigned i=0; i<ProgList->GetCount(); ++i) {
                if (i != 0) listString += wxT("; ");
                listString += ProgList->GetString(i);
            }
            return listString;
        }

        void AddProgram(wxString name) {
            int existing = ProgList->FindString(name);
            if (existing == wxNOT_FOUND) {
                existing = ProgList->Append(name);
            }
            ProgList->SetSelection(existing);
            RemoveButton->Enable();
        }

    private:
        void onRemoveButton(wxCommandEvent& event) {
            int selection = ProgList->GetSelection();
            if (selection != wxNOT_FOUND) {
                ProgList->Delete(selection);
            }
            onListSelChange(event);
        }

        void onListSelChange(wxCommandEvent&) {
            RemoveButton->Enable(ProgList->GetSelection() != wxNOT_FOUND);
        }

        void onActivate(wxActivateEvent& event) {
            if (!event.GetActive()) {
                // captured mouse is lost -- tidy up
                dragHandler.Deactivate();
            }
            event.Skip();
        }

    private:
        struct DragHandler : wxEvtHandler {
            DragHandler(ProgListDialog& dialog) : dialog(dialog) {
                dialog.DragIcon->SetBitmap(wxBitmap(wxT("crosshair_bitmap"), wxBITMAP_TYPE_BMP_RESOURCE));
                Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(DragHandler::onMouseDown));
                Connect(wxEVT_LEFT_UP, wxMouseEventHandler(DragHandler::onMouseUp));
                Connect(wxEVT_MOTION , wxMouseEventHandler(DragHandler::onMouseMove));
            }
            
            void Deactivate() {
                dialog.DragIcon->SetCursor(wxNullCursor);
                dialog.DragIcon->SetBitmap(wxBitmap(wxT("crosshair_bitmap"), wxBITMAP_TYPE_BMP_RESOURCE));
                if (dialog.DragIcon->HasCapture()) dialog.DragIcon->ReleaseMouse();
                win32funcs::UnHighlightWindow();
            }

        private:
            void onMouseDown(wxMouseEvent& event) {
                dialog.DragIcon->CaptureMouse(); 
                dialog.DragIcon->SetBitmap(wxBitmap(wxT("nocrosshair_bitmap"), wxBITMAP_TYPE_BMP_RESOURCE));
                dialog.DragIcon->SetCursor(wxCursor(wxT("crosshair_cursor")));
                event.Skip();
            }

            void onMouseUp(wxMouseEvent& event) {
                if (dialog.DragIcon->HasCapture()) {
                    Deactivate();
                    wxPoint scrPos = dialog.DragIcon->ClientToScreen(event.GetPosition());
                    std::wstring progName = win32funcs::GetExeForMousePos(scrPos.x, scrPos.y);
                    dialog.AddProgram(progName.c_str());
                }
                event.Skip();
            }

            void onMouseMove(wxMouseEvent& event) {
                if (event.LeftIsDown()) {
                    wxPoint scrPos = dialog.DragIcon->ClientToScreen(event.GetPosition());
                    win32funcs::HighlightWindow(scrPos.x, scrPos.y);
                }
                event.Skip();
            }

            ProgListDialog& dialog; 
        };

        DragHandler dragHandler;
    };
}


bool EditProgList(wxString& programsInOut, wxWindow* parent) {
    ProgListDialog dlg(parent, programsInOut);
    if (dlg.ShowModal() == wxID_OK) {
        programsInOut = dlg.GetPrograms();
        return true;
    }
    else {
        return false;
    }
}
