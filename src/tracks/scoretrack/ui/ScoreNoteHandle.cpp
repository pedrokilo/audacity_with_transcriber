/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreNoteHandle.h"

#include "ScoreAudio.h"
#include "ScoreKeyboard.h"
#include "ScoreLayout.h"
#include "ScoreSound.h"
#include "ScoreTrack.h"

#include "../../../HitTestResult.h"
#include "../../../RefreshCode.h"
#include "../../../TrackPanelMouseEvent.h"
#include "ProjectHistory.h"
#include "ProjectTimeSignature.h"
#include "ViewInfo.h"

#include <wx/cursor.h>
#include <wx/event.h>

ScoreNoteHandle::ScoreNoteHandle(
   const std::shared_ptr<ScoreTrack> &pTrack, const wxRect &rect)
   : mpTrack{ pTrack }
   , mRect{ rect }
{
}

ScoreNoteHandle::~ScoreNoteHandle() = default;

UIHandlePtr ScoreNoteHandle::HitTest(
   std::weak_ptr<ScoreNoteHandle> &holder,
   const wxMouseState &state, const wxRect &rect,
   const std::shared_ptr<ScoreTrack> &pTrack)
{
   if (!pTrack || !pTrack->GetSelected())
      return {};
   if (state.ControlDown() || state.ShiftDown() || state.AltDown())
      return {};
   auto result = std::make_shared<ScoreNoteHandle>(pTrack, rect);
   return AssignUIHandlePtr(holder, result);
}

UIHandle::Result ScoreNoteHandle::Click(
   const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   using namespace RefreshCode;
   const wxMouseEvent &event = evt.event;
   if (!event.LeftDown() || !mpTrack)
      return Cancelled;

   auto &track = *mpTrack;
   const auto &viewInfo = ViewInfo::Get(*pProject);
   const double tempo = ProjectTimeSignature::Get(*pProject).GetTempo();
   const ScoreInput input = ScoreKeyboard::Input();

   const auto layout = ScoreLayout::For(mRect, track.GetStaffLines());
   const int x = event.m_x;
   const int y = event.m_y;

   // The clef, key and time signature area is not part of the music
   if (x < mRect.x + ScoreHeaderWidth(track))
      return RefreshNone;

   // Bar line mode: draw a bar line, or remove the one that was clicked
   if (input.barLine) {
      const double time = viewInfo.PositionToTime(x, mRect.x);
      const double tolerance = std::abs(
         viewInfo.PositionToTime(x + 5, mRect.x) - time);
      if (track.RemoveBarLineNear(time, tolerance)) {
         ProjectHistory::Get(*pProject)
            .PushState(XO("Removed a bar line"), XO("Remove Bar Line"));
      }
      else {
         // A bar line drawn close to a split of the audio sticks to it
         double barTime = std::max(0.0, time);
         double best = std::abs(
            viewInfo.PositionToTime(x + 8, mRect.x) - time);
         for (double split : ScoreAudioSplitTimes(*pProject)) {
            if (std::abs(split - time) <= best) {
               best = std::abs(split - time);
               barTime = split;
            }
         }
         track.AddBarLine(barTime);
         ProjectHistory::Get(*pProject)
            .PushState(XO("Added a bar line"), XO("Add Bar Line"));
      }
      return RefreshAll;
   }

   // Clicking an existing note removes it
   const auto &notes = track.GetNotes();
   const int bottomDia = ScoreTheory::BottomLineDia(track.GetClef());
   for (size_t i = 0; i < notes.size(); ++i) {
      const auto &note = notes[i];
      const int nx = static_cast<int>(viewInfo.TimeToPosition(note.time, mRect.x));
      if (std::abs(nx - x) > ScoreLayout::Space)
         continue;
      const bool hit = note.rest
         ? std::abs(y - layout.StaffCenter()) <= ScoreLayout::Space * 2
         : std::abs(layout.StepToY(note.dia - bottomDia) - y) <= ScoreLayout::Space / 2 + 1;
      if (hit) {
         track.RemoveNote(i);
         ProjectHistory::Get(*pProject)
            .PushState(XO("Removed a note"), XO("Remove Note"));
         return RefreshAll;
      }
   }

   // Otherwise enter a note exactly where it was clicked
   const int step = layout.YToStep(y);
   ScoreNote note;
   note.time = std::max(0.0, viewInfo.PositionToTime(x, mRect.x));
   note.duration = input.duration;
   note.dots = input.dots;
   note.rest = input.rest;
   note.length = ScoreTheory::LengthInSeconds(input.duration, input.dots, tempo);
   note.tie = input.tie && !input.rest;
   note.dia = bottomDia + step;
   note.accidental = input.accidental;
   switch (input.accidental) {
   case ScoreAccidental::Natural: note.alter = 0; break;
   case ScoreAccidental::Sharp:   note.alter = 1; break;
   case ScoreAccidental::Flat:    note.alter = -1; break;
   default:
      note.alter = ScoreTheory::KeyAlteration(track.GetKey(), note.dia % 7);
      break;
   }
   track.AddNote(note);

   ProjectHistory::Get(*pProject)
      .PushState(XO("Added a note"), XO("Add Note"));

   if (!note.rest) {
      ScoreSound::PlayNote(
         ScoreTheory::Midi(note.dia, note.alter),
         ScoreTheory::LengthInSeconds(note.duration, note.dots, tempo));
   }
   return RefreshAll;
}

UIHandle::Result ScoreNoteHandle::Drag(
   const TrackPanelMouseEvent &, AudacityProject *)
{
   return RefreshCode::RefreshNone;
}

HitTestPreview ScoreNoteHandle::Preview(
   const TrackPanelMouseState &, AudacityProject *)
{
   static wxCursor cursor{ wxCURSOR_PENCIL };
   return {
      XO("Click to add a note, click a note to remove it. "
         "Ctrl, Shift or Alt: select"),
      &cursor
   };
}

UIHandle::Result ScoreNoteHandle::Release(
   const TrackPanelMouseEvent &, AudacityProject *, wxWindow *)
{
   return RefreshCode::RefreshNone;
}

UIHandle::Result ScoreNoteHandle::Cancel(AudacityProject *)
{
   return RefreshCode::RefreshNone;
}

std::shared_ptr<const Track> ScoreNoteHandle::FindTrack() const
{
   return mpTrack;
}
