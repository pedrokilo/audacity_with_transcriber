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

#include <algorithm>
#include <functional>

#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
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

// A bitmap of something drawn in black, centred on what was actually inked:
// the text of a button is centred by its line box, which cuts the stems and
// flags of the tall music symbols.
wxBitmap InkBitmap(const wxSize &size,
   const std::function<void(wxDC &, const wxSize &)> &draw)
{
   // Work on a larger canvas, white on black ink, and look for the ink
   const wxSize canvas(size.x * 3, size.y * 3);
   wxBitmap work(canvas.x, canvas.y, 24);
   {
      wxMemoryDC dc(work);
      dc.SetBackground(*wxWHITE_BRUSH);
      dc.Clear();
      draw(dc, canvas);
      dc.SelectObject(wxNullBitmap);
   }
   const wxImage image = work.ConvertToImage();

   int minX = canvas.x, minY = canvas.y, maxX = -1, maxY = -1;
   for (int y = 0; y < canvas.y; ++y)
      for (int x = 0; x < canvas.x; ++x)
         if (image.GetRed(x, y) < 200) {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
         }

   wxImage out(size.x, size.y);
   out.InitAlpha();
   for (int y = 0; y < size.y; ++y)
      for (int x = 0; x < size.x; ++x) {
         out.SetRGB(x, y, 20, 20, 24);
         out.SetAlpha(x, y, 0);
      }
   if (maxX >= minX) {
      const int offsetX = (size.x - (maxX - minX + 1)) / 2 - minX;
      const int offsetY = (size.y - (maxY - minY + 1)) / 2 - minY;
      for (int y = minY; y <= maxY; ++y)
         for (int x = minX; x <= maxX; ++x) {
            const int tx = x + offsetX, ty = y + offsetY;
            if (tx >= 0 && tx < size.x && ty >= 0 && ty < size.y)
               out.SetAlpha(tx, ty, static_cast<unsigned char>(255 - image.GetRed(x, y)));
         }
   }
   return wxBitmap(out);
}

//! A music glyph centred in a bitmap
wxBitmap GlyphBitmap(int glyph, int pixels, const wxSize &size)
{
   return InkBitmap(size, [=](wxDC &dc, const wxSize &canvas) {
      dc.SetFont(ScoreFont::Get(pixels));
      dc.SetTextForeground(*wxBLACK);
      ScoreFont::Draw(dc, glyph, canvas.x / 3, canvas.y * 2 / 3);
   });
}

constexpr int ButtonSize = 44;
const wxSize IconSize(ButtonSize - 8, ButtonSize - 8);

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
   for (int i = 0; i < static_cast<int>(ScoreDuration::Count); ++i) {
      auto *button = new wxToggleButton(panel, wxID_ANY, wxEmptyString,
         wxDefaultPosition, wxSize(ButtonSize, ButtonSize));
      button->SetBitmap(GlyphBitmap(DurationGlyphs[i], 30, IconSize));
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
   auto addToggle = [&](wxToggleButton *&slot, const wxBitmap &bitmap, const char *tip) {
      slot = new wxToggleButton(panel, wxID_ANY, wxEmptyString,
         wxDefaultPosition, wxSize(ButtonSize, ButtonSize));
      slot->SetBitmap(bitmap);
      slot->SetToolTip(S(tip));
      slot->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) { OnToggles(); });
      modifiers->Add(slot, 0, wxALL, 2);
   };
   // The dot is a very small glyph: draw it larger
   addToggle(mDot, GlyphBitmap(ScoreFont::AugmentationDot, 56, IconSize), "Puntillo");
   addToggle(mRest, GlyphBitmap(ScoreFont::RestQuarter, 30, IconSize), "Silencio");
   addToggle(mTie, InkBitmap(IconSize, [](wxDC &dc, const wxSize &canvas) {
      dc.SetPen(wxPen(*wxBLACK, 2));
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawEllipticArc(canvas.x / 2 - 16, canvas.y / 2 - 8, 32, 16, 180, 360);
   }), "Ligadura con la nota siguiente");
   addToggle(mBar, InkBitmap(IconSize, [](wxDC &dc, const wxSize &canvas) {
      dc.SetPen(wxPen(*wxBLACK, 2));
      dc.DrawLine(canvas.x / 2, canvas.y / 2 - 14, canvas.x / 2, canvas.y / 2 + 14);
   }), "Barra de comp\xC3\xA1s: clic en el pentagrama para trazarla o quitarla");
   root->Add(modifiers, 0, wxLEFT | wxRIGHT, 4);

   // --- Accidentals
   auto *accidentals = new wxBoxSizer(wxHORIZONTAL);
   const int accGlyphs[] = {
      ScoreFont::AccidentalNatural, ScoreFont::AccidentalSharp, ScoreFont::AccidentalFlat };
   const char *accTips[] = { "Becuadro", "Sostenido", "Bemol" };
   for (int i = 0; i < 3; ++i) {
      mAcc[i] = new wxToggleButton(panel, wxID_ANY, wxEmptyString,
         wxDefaultPosition, wxSize(ButtonSize, ButtonSize));
      mAcc[i]->SetBitmap(GlyphBitmap(accGlyphs[i], 30, IconSize));
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
