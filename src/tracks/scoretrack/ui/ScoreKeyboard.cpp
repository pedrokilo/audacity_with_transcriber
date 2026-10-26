/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreKeyboard.h"

#include "ActiveProject.h"
#include "ScoreAudio.h"
#include "ScoreFont.h"
#include "Project.h"
#include "ProjectHistory.h"
#include "../../../ProjectWindows.h"
#include "../../../TrackPanel.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/font.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>

namespace {
wxWeakRef<ScoreKeyboard> &Instance()
{
   static wxWeakRef<ScoreKeyboard> instance;
   return instance;
}

wxString S(const char *utf8)
{
   return wxString::FromUTF8(utf8);
}

// Musical symbols live in the Segoe UI Symbol font on Windows
wxFont SymbolFont(int pixels)
{
   wxFont font(wxFontInfo(wxSize(0, pixels)).FaceName("Segoe UI Symbol"));
   return font;
}

struct TimeChoice { int num; int den; };
constexpr TimeChoice TimeChoices[] = { { 6, 8 }, { 4, 4 }, { 2, 4 }, { 3, 4 } };

const int DurationGlyphs[] = {
   ScoreFont::NoteWhole, ScoreFont::NoteHalfUp, ScoreFont::NoteQuarterUp,
   ScoreFont::Note8thUp, ScoreFont::Note16thUp, ScoreFont::Note32ndUp,
};
const char *DurationTips[] = {
   "Redonda", "Blanca", "Negra", "Corchea", "Semicorchea", "Fusa" };

std::vector<ScoreTrack *> SelectedScoreTracks(AudacityProject &project)
{
   std::vector<ScoreTrack *> result;
   for (auto track : TrackList::Get(project).Selected<ScoreTrack>())
      result.push_back(track);
   return result;
}
}

ScoreKeyboard::ScoreKeyboard(wxWindow *parent)
   : wxMiniFrame{ parent, wxID_ANY, S("Teclado de partitura"),
        wxDefaultPosition, wxDefaultSize,
        wxCAPTION | wxCLOSE_BOX | wxFRAME_FLOAT_ON_PARENT | wxFRAME_TOOL_WINDOW }
{
   auto *panel = new wxPanel(this);
   auto *root = new wxBoxSizer(wxVERTICAL);

   // --- Clef, key and time signature
   auto *grid = new wxFlexGridSizer(2, 4, 6);
   grid->AddGrowableCol(1);

   mClef = new wxChoice(panel, wxID_ANY);
   mClef->Append(S("Sol"));
   mClef->Append(S("Fa"));
   grid->Add(new wxStaticText(panel, wxID_ANY, S("Clave")), 0, wxALIGN_CENTER_VERTICAL);
   grid->Add(mClef, 1, wxEXPAND);

   mKey = new wxChoice(panel, wxID_ANY);
   for (const char *name : { "Do", "Do menor", "La", "La menor",
                             "Sol", "Sol menor", "Fa", "Fa menor" })
      mKey->Append(S(name));
   grid->Add(new wxStaticText(panel, wxID_ANY, S("Armadura")), 0, wxALIGN_CENTER_VERTICAL);
   grid->Add(mKey, 1, wxEXPAND);

   mTime = new wxChoice(panel, wxID_ANY);
   for (const auto &t : TimeChoices)
      mTime->Append(wxString::Format("%d/%d", t.num, t.den));
   grid->Add(new wxStaticText(panel, wxID_ANY, S("Comp\xC3\xA1s")), 0, wxALIGN_CENTER_VERTICAL);
   grid->Add(mTime, 1, wxEXPAND);

   root->Add(grid, 0, wxEXPAND | wxALL, 6);

   for (auto *choice : { mClef, mKey, mTime })
      choice->Bind(wxEVT_CHOICE, &ScoreKeyboard::OnSettingChoice, this);

   // --- Note values
   auto *durations = new wxBoxSizer(wxHORIZONTAL);
   const wxFont noteFont = ScoreFont::Get(26);
   for (int i = 0; i < static_cast<int>(ScoreDuration::Count); ++i) {
      auto *button = new wxToggleButton(panel, wxID_ANY,
         ScoreFont::Text(DurationGlyphs[i]),
         wxDefaultPosition, wxSize(44, 44));
      button->SetFont(noteFont);
      button->SetToolTip(S(DurationTips[i]));
      button->SetValue(i == static_cast<int>(mInput.duration));
      button->Bind(wxEVT_TOGGLEBUTTON, [this, i](wxCommandEvent &) {
         OnDuration(i);
      });
      mDuration[i] = button;
      durations->Add(button, 0, wxALL, 2);
   }
   root->Add(durations, 0, wxLEFT | wxRIGHT, 4);

   // --- Dot, rest, tie
   auto *modifiers = new wxBoxSizer(wxHORIZONTAL);
   const wxFont smallSymbols = SymbolFont(22);
   auto addToggle = [&](wxToggleButton *&slot, const char *label, const char *tip,
                        const wxFont &font) {
      slot = new wxToggleButton(panel, wxID_ANY, S(label),
         wxDefaultPosition, wxSize(44, 44));
      slot->SetFont(font);
      slot->SetToolTip(S(tip));
      slot->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) { OnToggles(); });
      modifiers->Add(slot, 0, wxALL, 2);
   };
   addToggle(mDot, "\xE2\x80\xA2", "Puntillo", smallSymbols);
   addToggle(mRest, "\xEE\x93\xA5", "Silencio", noteFont);   // U+E4E5 quarter rest
   addToggle(mTie, "\xE2\x8C\xA3", "Ligadura con la nota siguiente", smallSymbols);
   addToggle(mBar, "|", "Barra de comp\xC3\xA1s: clic en el pentagrama para trazarla o quitarla", smallSymbols);
   root->Add(modifiers, 0, wxLEFT | wxRIGHT, 4);

   // --- Accidentals
   auto *accidentals = new wxBoxSizer(wxHORIZONTAL);
   const char *accLabels[] = { "\xEE\x89\xA1", "\xEE\x89\xA2", "\xEE\x89\xA0" };   // natural, sharp, flat
   const char *accTips[] = { "Becuadro", "Sostenido", "Bemol" };
   for (int i = 0; i < 3; ++i) {
      mAcc[i] = new wxToggleButton(panel, wxID_ANY, S(accLabels[i]),
         wxDefaultPosition, wxSize(44, 44));
      mAcc[i]->SetFont(ScoreFont::Get(24));
      mAcc[i]->SetToolTip(S(accTips[i]));
      mAcc[i]->Bind(wxEVT_TOGGLEBUTTON, [this, i](wxCommandEvent &) {
         for (int j = 0; j < 3; ++j)
            if (j != i)
               mAcc[j]->SetValue(false);
         OnToggles();
      });
      accidentals->Add(mAcc[i], 0, wxALL, 2);
   }
   root->Add(accidentals, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);

   auto *fromSplits = new wxButton(panel, wxID_ANY,
      S("Barras en los cortes del audio"));
   fromSplits->SetToolTip(S("Traza una barra de comp\xC3\xA1s en cada corte del audio"));
   fromSplits->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnBarsFromSplits(); });
   root->Add(fromSplits, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

   panel->SetSizer(root);
   auto *frameSizer = new wxBoxSizer(wxVERTICAL);
   frameSizer->Add(panel, 1, wxEXPAND);
   SetSizerAndFit(frameSizer);

   // Put the palette in the top right corner of the project window
   if (parent) {
      const auto r = parent->GetScreenRect();
      Move(r.GetRight() - GetSize().GetWidth() - 24, r.GetTop() + 120);
   }
}

ScoreKeyboard::~ScoreKeyboard() = default;

void ScoreKeyboard::OnDuration(int index)
{
   for (int i = 0; i < static_cast<int>(ScoreDuration::Count); ++i)
      mDuration[i]->SetValue(i == index);
   mInput.duration = static_cast<ScoreDuration>(index);
}

void ScoreKeyboard::OnBarsFromSplits()
{
   auto pProject = GetActiveProject().lock();
   if (!pProject)
      return;
   const auto splits = ScoreAudioSplitTimes(*pProject);
   bool changed = false;
   for (auto *track : SelectedScoreTracks(*pProject)) {
      for (double t : splits) {
         if (t > 1e-4) {
            track->AddBarLine(t);
            changed = true;
         }
      }
   }
   if (changed) {
      ProjectHistory::Get(*pProject)
         .PushState(XO("Added bar lines at the audio splits"),
                    XO("Add Bar Lines"));
      TrackPanel::Get(*pProject).Refresh(false);
   }
}

void ScoreKeyboard::OnToggles()
{
   mInput.dots = mDot->GetValue() ? 1 : 0;
   mInput.rest = mRest->GetValue();
   mInput.tie = mTie->GetValue();
   mInput.barLine = mBar->GetValue();
   mInput.accidental = ScoreAccidental::None;
   if (mAcc[0]->GetValue())
      mInput.accidental = ScoreAccidental::Natural;
   else if (mAcc[1]->GetValue())
      mInput.accidental = ScoreAccidental::Sharp;
   else if (mAcc[2]->GetValue())
      mInput.accidental = ScoreAccidental::Flat;
}

void ScoreKeyboard::SyncFromTrack(const ScoreTrack &track)
{
   mSyncing = true;
   mClef->SetSelection(track.GetClef() == ScoreClef::Bass ? 1 : 0);
   mKey->SetSelection(static_cast<int>(track.GetKey()));
   int timeIndex = -1;
   for (int i = 0; i < 4; ++i)
      if (TimeChoices[i].num == track.GetTimeSignature().numerator
          && TimeChoices[i].den == track.GetTimeSignature().denominator)
         timeIndex = i;
   mTime->SetSelection(timeIndex);
   mSyncing = false;
}

void ScoreKeyboard::OnSettingChoice(wxCommandEvent &)
{
   if (mSyncing)
      return;
   auto pProject = GetActiveProject().lock();
   if (!pProject)
      return;

   const auto clef = mClef->GetSelection() == 1 ? ScoreClef::Bass : ScoreClef::Treble;
   const auto key = static_cast<ScoreKey>(std::max(0, mKey->GetSelection()));
   const int t = mTime->GetSelection();

   bool changed = false;
   for (auto *track : SelectedScoreTracks(*pProject)) {
      track->SetClef(clef);
      track->SetKey(key);
      if (t >= 0)
         track->SetTimeSignature({ TimeChoices[t].num, TimeChoices[t].den });
      changed = true;
   }
   if (changed) {
      ProjectHistory::Get(*pProject)
         .PushState(XO("Changed score clef, key or time signature"),
                    XO("Score Settings"));
      TrackPanel::Get(*pProject).Refresh(false);
   }
}

void ScoreKeyboard::Update()
{
   auto pProject = GetActiveProject().lock();
   auto &instance = Instance();

   std::vector<ScoreTrack *> selected;
   if (pProject)
      selected = SelectedScoreTracks(*pProject);

   if (selected.empty()) {
      if (instance)
         instance->Hide();
      return;
   }

   wxWindow *frame = &GetProjectFrame(*pProject);
   if (instance && instance->GetParent() != frame) {
      instance->Destroy();
      instance = nullptr;
   }
   if (!instance)
      instance = new ScoreKeyboard(frame);

   instance->SyncFromTrack(*selected.front());
   if (!instance->IsShown())
      instance->Show(true);
}

ScoreInput ScoreKeyboard::Input()
{
   auto &instance = Instance();
   return instance ? instance->mInput : ScoreInput{};
}
