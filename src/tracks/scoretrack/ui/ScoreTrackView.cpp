/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrackView.h"

#include "ScoreFont.h"
#include "ScoreKeyboard.h"
#include "ScoreLayout.h"
#include "ScoreNoteHandle.h"
#include "ScoreTrack.h"
#include "ScoreTrackVRulerControls.h"

#include "AColor.h"
#include "AllThemeResources.h"
#include "PendingTracks.h"
#include "Project.h"
#include "SyncLock.h"
#include "Theme.h"
#include "ViewInfo.h"
#include "../../../HitTestResult.h"
#include "../../../TrackArt.h"
#include "../../../TrackArtist.h"
#include "../../../TrackPanel.h"
#include "../../../TrackPanelDrawingContext.h"
#include "../../../TrackPanelMouseEvent.h"

#include <algorithm>
#include <cmath>

#include <wx/app.h>
#include <wx/dc.h>

namespace {
// Position (in steps from the bottom line) of each accidental of a key
// signature, in the order they are added.
const int TrebleSharps[7] = { 8, 5, 9, 6, 3, 7, 4 };
const int TrebleFlats[7] = { 4, 7, 3, 6, 2, 5, 1 };
const int BassSharps[7] = { 6, 3, 7, 4, 1, 5, 2 };
const int BassFlats[7] = { 2, 5, 1, 4, 0, 3, 6 };

// Widths of glyphs, in staff spaces (from the font metadata)
constexpr double HeadWidth = 1.3;
constexpr double WholeHeadWidth = 1.492;
constexpr double RestWidth = 1.3;

double HeadWidthOf(ScoreDuration d)
{
   return d == ScoreDuration::Whole ? WholeHeadWidth : HeadWidth;
}

int RestGlyph(ScoreDuration d)
{
   switch (d) {
   case ScoreDuration::Whole: return ScoreFont::RestWhole;
   case ScoreDuration::Half: return ScoreFont::RestHalf;
   case ScoreDuration::Quarter: return ScoreFont::RestQuarter;
   case ScoreDuration::Eighth: return ScoreFont::Rest8th;
   case ScoreDuration::Sixteenth: return ScoreFont::Rest16th;
   default: return ScoreFont::Rest32nd;
   }
}

// Rests hang from the fourth line when they are whole, sit on the middle line
// when they are half, and are centred on the staff otherwise.
int RestOriginStep(ScoreDuration d)
{
   return d == ScoreDuration::Whole ? 6 : 4;
}

void DrawRest(wxDC &dc, const ScoreLayout &layout, ScoreDuration duration,
   int dots, int centerX, const wxColour &ink)
{
   const int s = layout.space;
   const int width = layout.Spaces(RestWidth);
   const int x = centerX - width / 2;
   dc.SetTextForeground(ink);
   ScoreFont::Draw(dc, RestGlyph(duration), x,
      layout.StepToY(RestOriginStep(duration)));
   for (int d = 0; d < dots; ++d)
      ScoreFont::Draw(dc, ScoreFont::AugmentationDot,
         x + width + layout.Spaces(0.3 + 0.5 * d), layout.StepToY(5));
   (void)s;
}

void DrawNote(wxDC &dc, const ScoreLayout &layout, const ScoreTrack &track,
   const ScoreNote &note, int centerX, int nextX, const wxColour &ink,
   bool withStem = true)
{
   const int s = layout.space;
   dc.SetPen(wxPen(ink, 1));
   dc.SetTextForeground(ink);

   if (note.rest) {
      DrawRest(dc, layout, note.duration, note.dots, centerX, ink);
      return;
   }

   const int step = note.dia - ScoreTheory::BottomLineDia(track.GetClef());
   const int y = layout.StepToY(step);
   const int headWidth = layout.Spaces(HeadWidthOf(note.duration));
   const int xLeft = centerX - headWidth / 2;

   // Ledger lines, a little wider than the head
   const int extension = layout.Spaces(0.33);
   if (step < 0) {
      for (int k = -2; k >= step; k -= 2)
         dc.DrawLine(xLeft - extension, layout.StepToY(k),
            xLeft + headWidth + extension, layout.StepToY(k));
   }
   else if (step > layout.TopLineStep()) {
      for (int k = layout.TopLineStep() + 2; k <= step; k += 2)
         dc.DrawLine(xLeft - extension, layout.StepToY(k),
            xLeft + headWidth + extension, layout.StepToY(k));
   }

   // Head
   const int headGlyph = note.duration == ScoreDuration::Whole
         ? ScoreFont::NoteheadWhole
      : note.duration == ScoreDuration::Half
         ? ScoreFont::NoteheadHalf : ScoreFont::NoteheadBlack;
   ScoreFont::Draw(dc, headGlyph, xLeft, y);

   // Stem and flags
   const bool stemUp = step < layout.TopLineStep() / 2;
   const int flags = std::max(0,
      static_cast<int>(note.duration) - static_cast<int>(ScoreDuration::Quarter));
   if (withStem && note.duration != ScoreDuration::Whole) {
      const int length = layout.Spaces(3.5 + std::max(0, flags - 1) * 0.9);
      const int stemX = stemUp ? xLeft + headWidth - 1 : xLeft;
      const int stemFrom = stemUp ? y - layout.Spaces(0.16) : y + layout.Spaces(0.168);
      const int stemTo = stemUp ? stemFrom - length : stemFrom + length;
      dc.DrawLine(stemX, stemFrom, stemX, stemTo);

      if (flags > 0) {
         int glyph;
         if (flags == 1)
            glyph = stemUp ? ScoreFont::Flag8thUp : ScoreFont::Flag8thDown;
         else if (flags == 2)
            glyph = stemUp ? ScoreFont::Flag16thUp : ScoreFont::Flag16thDown;
         else
            glyph = stemUp ? ScoreFont::Flag32ndUp : ScoreFont::Flag32ndDown;
         ScoreFont::Draw(dc, glyph, stemX, stemTo);
      }
   }

   // Dots sit in the space, never on a line
   const int dotStep = (step % 2 == 0) ? step + 1 : step;
   for (int d = 0; d < note.dots; ++d)
      ScoreFont::Draw(dc, ScoreFont::AugmentationDot,
         xLeft + headWidth + layout.Spaces(0.4 + 0.5 * d),
         layout.StepToY(dotStep));

   // Written accidental, to the left of the head
   if (note.accidental != ScoreAccidental::None) {
      const int glyph = note.accidental == ScoreAccidental::Sharp
            ? ScoreFont::AccidentalSharp
         : note.accidental == ScoreAccidental::Flat
            ? ScoreFont::AccidentalFlat : ScoreFont::AccidentalNatural;
      ScoreFont::Draw(dc, glyph, xLeft - layout.Spaces(1.3), y);
   }

   // Tie to the next note of the same pitch
   if (note.tie && nextX > centerX) {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      const int h = layout.Spaces(0.7);
      const int xa = xLeft + headWidth;
      const int w = std::max(4, nextX - centerX - headWidth);
      if (stemUp)
         dc.DrawEllipticArc(xa, y + s / 2 - h, w, 2 * h, 180, 360);
      else
         dc.DrawEllipticArc(xa, y - s / 2 - h, w, 2 * h, 0, 180);
   }
}
}

namespace {
//! The bar each note falls in and where it starts inside it, in 32nd notes.
//!
//! Notes are placed by hand, so for the rhythm (rests and beams) they are
//! moved onto a grid, one after the other inside a bar: every note goes to the
//! grid slot of its own value (an eighth begins on a multiple of an eighth) but
//! never before the end of the previous note, and a gap is only left where the
//! notes really are far apart. So six eighth notes placed a little unevenly
//! still make six eighth notes, with no rest in between.
struct Rhythm {
   std::vector<int> bar;     //!< -1: outside every bar
   std::vector<int> start;
};

Rhythm AnalyzeRhythm(const ScoreTrack &track)
{
   const auto &notes = track.GetNotes();
   const auto measures = track.GetMeasures();
   const int barUnits = ScoreTheory::MeasureUnits(track.GetTimeSignature());
   Rhythm rhythm{ std::vector<int>(notes.size(), -1), std::vector<int>(notes.size(), -1) };

   for (size_t m = 0; m < measures.size(); ++m) {
      const double length = measures[m].end - measures[m].start;
      if (length <= 0.0)
         continue;
      int previousEnd = 0;
      int previousStart = 0;
      double previousTime = -1.0;
      for (size_t i = 0; i < notes.size(); ++i) {
         const auto &note = notes[i];
         if (note.time < measures[m].start - 1e-6 || note.time >= measures[m].end - 1e-6)
            continue;
         rhythm.bar[i] = static_cast<int>(m);

         const int units = ScoreTheory::NoteUnits(note.duration, note.dots);
         int start;
         if (previousTime >= 0.0 && std::abs(note.time - previousTime) < 1e-6)
            start = previousStart;                       // notes of a chord
         else {
            const double raw = (note.time - measures[m].start) / length * barUnits;
            const int grid = std::max(1,
               ScoreTheory::UnitsPerWhole >> static_cast<int>(note.duration));
            start = static_cast<int>(std::lround(raw / grid)) * grid;
            start = std::max(start, previousEnd);
            // Not far enough from the previous note to be a rest
            if (start > previousEnd && raw < previousEnd + 0.75 * grid)
               start = previousEnd;
         }
         start = std::clamp(start, 0, std::max(0, barUnits - units));
         rhythm.start[i] = start;
         previousStart = start;
         previousTime = note.time;
         previousEnd = std::max(previousEnd, start + units);
      }
   }
   return rhythm;
}

bool Beamable(const ScoreNote &note)
{
   return !note.rest && note.duration >= ScoreDuration::Eighth;
}

//! Groups of consecutive notes that are joined by beams, as MuseScore does:
//! eighth notes and shorter within one beat of a bar. Where there are no bar
//! lines, notes that follow each other closely are joined.
std::vector<std::vector<size_t>> BeamGroups(const ScoreTrack &track, const Rhythm &rhythm)
{
   const auto &notes = track.GetNotes();
   const auto ts = track.GetTimeSignature();
   const bool compound = ts.denominator == 8 && ts.numerator >= 6
      && ts.numerator % 3 == 0;
   const int beatUnits = compound ? 12 : std::max(1, ScoreTheory::UnitsPerWhole / ts.denominator);

   // Which bar and beat a note falls in; bar is -1 outside every bar
   auto place = [&](size_t index) {
      if (rhythm.bar[index] < 0)
         return std::make_pair(-1, -1);
      return std::make_pair(rhythm.bar[index], rhythm.start[index] / beatUnits);
   };

   std::vector<std::vector<size_t>> groups;
   std::vector<size_t> current;
   auto flush = [&] {
      if (current.size() >= 2)
         groups.push_back(current);
      current.clear();
   };

   for (size_t i = 0; i < notes.size(); ++i) {
      if (!Beamable(notes[i])) {
         flush();
         continue;
      }
      if (!current.empty()) {
         const auto &previous = notes[current.back()];
         bool join = notes[i].time > previous.time + 1e-6;
         if (join) {
            const auto a = place(current.back());
            const auto b = place(i);
            if (a.first >= 0 || b.first >= 0)
               join = a == b;
            else
               join = notes[i].time - previous.time <= 1.5 * previous.length;
         }
         if (!join)
            flush();
      }
      current.push_back(i);
   }
   flush();
   return groups;
}

//! Draws the stems and beams of a group of notes (heads are drawn apart)
void DrawBeamGroup(wxDC &dc, const ScoreLayout &layout, const ScoreTrack &track,
   const std::vector<ScoreNote> &notes, const std::vector<size_t> &group,
   const std::vector<int> &centers, const wxColour &ink)
{
   const int s = layout.space;
   const int bottomDia = ScoreTheory::BottomLineDia(track.GetClef());
   const int middle = layout.TopLineStep() / 2;

   // The stems go up unless the notes lie, on the whole, above the middle line
   int balance = 0;
   for (size_t index : group)
      balance += (notes[index].dia - bottomDia) - middle;
   const bool up = balance <= 0;

   struct Stem { int x; int y0; int level; };
   std::vector<Stem> stems;
   for (size_t index : group) {
      const auto &note = notes[index];
      const int step = note.dia - bottomDia;
      const int y = layout.StepToY(step);
      const int xLeft = centers[index] - layout.Spaces(HeadWidth) / 2;
      const int x = up ? xLeft + layout.Spaces(HeadWidth) - 1 : xLeft;
      const int y0 = up ? y - layout.Spaces(0.16) : y + layout.Spaces(0.168);
      stems.push_back({ x, y0, static_cast<int>(note.duration) - static_cast<int>(ScoreDuration::Quarter) });
   }

   // The beam runs from the first to the last stem, following the notes but
   // never steeper than a space and a half
   const int length = layout.Spaces(3.5);
   const int sign = up ? -1 : 1;
   const int xFirst = stems.front().x;
   const int xLast = stems.back().x;
   double yFirst = stems.front().y0 + sign * length;
   double yLast = stems.back().y0 + sign * length;
   const double limit = 1.5 * s;
   if (std::abs(yLast - yFirst) > limit)
      yLast = yFirst + (yLast > yFirst ? limit : -limit);
   const double slope = xLast > xFirst ? (yLast - yFirst) / (xLast - xFirst) : 0.0;
   auto beamY = [&](int x) { return yFirst + slope * (x - xFirst); };

   // Every stem at least two and a half spaces long
   const double minimum = 2.5 * s;
   double shift = 0.0;
   for (const auto &stem : stems) {
      const double stemLength = std::abs(beamY(stem.x) - stem.y0);
      if (stemLength < minimum)
         shift = std::max(shift, minimum - stemLength);
   }
   yFirst += sign * shift;
   yLast += sign * shift;

   dc.SetPen(wxPen(ink, 1));
   for (const auto &stem : stems)
      dc.DrawLine(stem.x, stem.y0, stem.x, static_cast<int>(std::lround(beamY(stem.x))));

   // Beams: one for every level of shortness
   const double thickness = layout.Spaces(0.5);
   const double gap = layout.Spaces(0.25);
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(wxBrush(ink));
   auto drawBeam = [&](int xa, int xb, int level) {
      const double offset = -sign * level * (thickness + gap);
      const double ya = beamY(xa) + offset;
      const double yb = beamY(xb) + offset;
      const double t = -sign * thickness;
      wxPoint points[4] = {
         { xa, static_cast<int>(std::lround(ya)) },
         { xb, static_cast<int>(std::lround(yb)) },
         { xb, static_cast<int>(std::lround(yb + t)) },
         { xa, static_cast<int>(std::lround(ya + t)) },
      };
      dc.DrawPolygon(4, points);
   };

   // level 0 is the eighth beam; 16th notes add level 1, 32nd level 2
   for (int level = 0; level <= 2; ++level) {
      size_t k = 0;
      while (k < stems.size()) {
         if (stems[k].level < level + 1) {
            ++k;
            continue;
         }
         size_t end = k;
         while (end + 1 < stems.size() && stems[end + 1].level >= level + 1)
            ++end;
         if (end > k)
            drawBeam(stems[k].x, stems[end].x, level);
         else if (level > 0) {
            // A lone short note gets a stub towards its neighbour
            const int stub = layout.Spaces(1.0);
            if (k + 1 < stems.size())
               drawBeam(stems[k].x, stems[k].x + stub, level);
            else
               drawBeam(stems[k].x - stub, stems[k].x, level);
         }
         k = end + 1;
      }
   }
}
}

ScoreTrackView::ScoreTrackView(const std::shared_ptr<Channel> &pChannel)
   : CommonChannelView{ pChannel }
{
   BindTo(FindChannel<ScoreTrack>().get());
}

ScoreTrackView::~ScoreTrackView()
{
}

void ScoreTrackView::Reparent(
   const std::shared_ptr<Track> &parent, size_t iChannel)
{
   auto oldParent = FindChannel<ScoreTrack>();
   auto newParent = track_cast<ScoreTrack *>(parent.get());
   if (oldParent.get() != newParent)
      BindTo(newParent);
   CommonChannelView::Reparent(parent, iChannel);
}

void ScoreTrackView::BindTo(ScoreTrack *pTrack)
{
   // Replaces any subscription to another track
   mSubscription = pTrack->Subscribe([](const ScoreTrackEvent &) {
      // The selection changed: show or hide the floating palette
      if (wxTheApp)
         wxTheApp->CallAfter([] { ScoreKeyboard::Update(); });
   });
}

std::vector<UIHandlePtr> ScoreTrackView::DetailedHitTest(
   const TrackPanelMouseState &st, const AudacityProject *, int, bool)
{
   std::vector<UIHandlePtr> results;
   auto result = ScoreNoteHandle::HitTest(
      mNoteHandle, st.state, st.rect, FindChannel<ScoreTrack>());
   if (result)
      results.push_back(result);
   return results;
}

using DoGetScoreTrackView = DoGetView::Override<ScoreTrack>;
DEFINE_ATTACHED_VIRTUAL_OVERRIDE(DoGetScoreTrackView) {
   return [](ScoreTrack &track, size_t) {
      return std::make_shared<ScoreTrackView>(
         track.SharedPointer<ScoreTrack>());
   };
}

std::shared_ptr<ChannelVRulerControls> ScoreTrackView::DoGetVRulerControls()
{
   return std::make_shared<ScoreTrackVRulerControls>(shared_from_this());
}

auto ScoreTrackView::GetMenuItems(
   const wxRect &rect, const wxPoint *pPosition, AudacityProject *pProject)
      -> std::vector<MenuItem>
{
   if (pProject && pPosition) {
      // Put the cursor where the click was, and select this score
      auto &viewInfo = ViewInfo::Get(*pProject);
      const double t = std::max(0.0, viewInfo.PositionToTime(pPosition->x, rect.x));
      viewInfo.selectedRegion.setTimes(t, t);
      if (auto pTrack = FindChannel<ScoreTrack>())
         pTrack->SetSelected(true);
   }
   return {
      { L"Split", XO("Split Clip") },
      { L"Join", XO("Join Clips") },
      {},
      { L"TrackMute", XO("Mute/Unmute Track") },
   };
}

void ScoreTrackView::Draw(
   TrackPanelDrawingContext &context, const wxRect &rect, unsigned iPass)
{
   if (iPass == TrackArtist::PassTracks && FindChannel()) {
      auto &dc = context.dc;
      const auto artist = TrackArtist::Get(context);
      const auto &zoomInfo = *artist->pZoomInfo;
      const auto &pendingTracks = *artist->pPendingTracks;

      const auto &track = static_cast<const ScoreTrack &>(
         pendingTracks.SubstitutePendingChangedChannel(*FindChannel()));

      // A sheet of paper whatever the theme, so the notation is always black on
      // a light background. The project grid of beats is not drawn: the bars
      // of a score are the ones drawn by the user.
      const wxColour paper = track.GetSelected()
         ? wxColour(240, 244, 252) : wxColour(252, 252, 250);
      const wxColour selectedTime(196, 212, 244);
      const wxColour ink(24, 24, 28);

      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(paper));
      dc.DrawRectangle(rect);
      if (track.GetSelected()) {
         const auto &region = *artist->pSelectedRegion;
         if (region.t1() > region.t0()) {
            const int xa = std::max(rect.x,
               static_cast<int>(zoomInfo.TimeToPosition(region.t0(), rect.x)));
            const int xb = std::min(rect.x + rect.width,
               static_cast<int>(zoomInfo.TimeToPosition(region.t1(), rect.x)));
            if (xb > xa) {
               dc.SetBrush(wxBrush(selectedTime));
               dc.DrawRectangle(xa, rect.y, xb - xa, rect.height);
            }
         }
      }
      const auto layout = ScoreLayout::For(rect, track.GetStaffLines());
      const int space = layout.space;
      const int header = ScoreHeaderWidth(track);
      const auto timeSignature = track.GetTimeSignature();

      auto timeToX = [&](double t) {
         return static_cast<int>(zoomInfo.TimeToPosition(t, rect.x));
      };

      auto drawStaffLines = [&](int x0, int x1) {
         dc.SetPen(wxPen(ink, 1));
         for (int i = 0; i < layout.lines; ++i) {
            const int y = layout.bottom - i * space;
            dc.DrawLine(x0, y, x1, y);
         }
      };
      drawStaffLines(rect.x, rect.x + rect.width - 1);

      // Bar lines, drawn by the user
      dc.SetPen(wxPen(ink, 1));
      for (double t : track.GetBarLines()) {
         const int x = timeToX(t);
         if (x > rect.x + header && x < rect.x + rect.width)
            dc.DrawLine(x, layout.StaffTop(), x, layout.bottom);
      }

      dc.SetFont(ScoreFont::Get(layout.Spaces(4)));
      dc.SetTextForeground(ink);

      const auto &notes = track.GetNotes();

      // Rests that complete every bar, as MuseScore does: a bar with nothing in
      // it gets the rest of a whole bar; a bar with some notes gets rests in
      // the gaps, split at the beats.
      const int barUnits = ScoreTheory::MeasureUnits(timeSignature);
      const Rhythm rhythm = AnalyzeRhythm(track);
      const auto measures = track.GetMeasures();
      for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
         const auto &measure = measures[measureIndex];
         const int x0 = timeToX(measure.start);
         const int x1 = timeToX(measure.end);
         if (x1 < rect.x || x0 > rect.x + rect.width)
            continue;
         const double length = measure.end - measure.start;
         if (length <= 0.0)
            continue;

         struct Span { int from; int to; };
         std::vector<Span> occupied;
         for (size_t i = 0; i < notes.size(); ++i) {
            if (rhythm.bar[i] != static_cast<int>(measureIndex))
               continue;
            const int from = rhythm.start[i];
            const int to = std::min(barUnits,
               from + ScoreTheory::NoteUnits(notes[i].duration, notes[i].dots));
            occupied.push_back({ from, to });
         }
         std::sort(occupied.begin(), occupied.end(),
            [](const Span &a, const Span &b) { return a.from < b.from; });

         // Notes are placed by hand, so a gap smaller than a dotted sixteenth is
         // just imprecision, not music
         constexpr int MinimumGap = 3;
         std::vector<ScoreTheory::RestSlot> rests;
         int cursor = 0;
         for (const auto &span : occupied) {
            if (span.from - cursor >= MinimumGap) {
               auto gap = ScoreTheory::FillGap(cursor, span.from, timeSignature);
               rests.insert(rests.end(), gap.begin(), gap.end());
            }
            cursor = std::max(cursor, span.to);
         }
         if (barUnits - cursor >= MinimumGap) {
            auto gap = ScoreTheory::FillGap(cursor, barUnits, timeSignature);
            rests.insert(rests.end(), gap.begin(), gap.end());
         }

         for (const auto &rest : rests) {
            int centerX;
            if (rest.wholeMeasure)
               centerX = (std::max(x0, rect.x + header) + x1) / 2;
            else {
               const double t = measure.start + length * rest.start / barUnits;
               centerX = timeToX(t) + layout.Spaces(0.65);
            }
            if (centerX < rect.x + header)
               continue;
            DrawRest(dc, layout, rest.duration, rest.dots, centerX, ink);
         }
      }

      // Notes, with beams joining the eighth notes and shorter ones
      const auto groups = BeamGroups(track, AnalyzeRhythm(track));
      std::vector<bool> beamed(notes.size(), false);
      for (const auto &group : groups)
         for (size_t index : group)
            beamed[index] = true;

      std::vector<int> centers(notes.size(), 0);
      for (size_t i = 0; i < notes.size(); ++i)
         centers[i] = timeToX(notes[i].time);

      for (size_t i = 0; i < notes.size(); ++i) {
         const int x = centers[i];
         if (x < rect.x - 40 || x > rect.x + rect.width + 40)
            continue;
         int nextX = -1;
         if (notes[i].tie && !notes[i].rest) {
            for (size_t j = i + 1; j < notes.size(); ++j)
               if (!notes[j].rest && notes[j].dia == notes[i].dia) {
                  nextX = centers[j];
                  break;
               }
         }
         DrawNote(dc, layout, track, notes[i], x, nextX, ink, !beamed[i]);
      }
      for (const auto &group : groups) {
         if (centers[group.back()] < rect.x - 40
             || centers[group.front()] > rect.x + rect.width + 40)
            continue;
         DrawBeamGroup(dc, layout, track, notes, group, centers, ink);
      }

      // Clef, key signature and time signature stay at the left edge
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(paper));
      dc.DrawRectangle(rect.x, rect.y, header, rect.height);
      drawStaffLines(rect.x, rect.x + header);

      dc.SetFont(ScoreFont::Get(layout.Spaces(4)));
      dc.SetTextForeground(ink);
      const bool bass = track.GetClef() == ScoreClef::Bass;
      int x = rect.x + layout.Spaces(0.8);
      ScoreFont::Draw(dc, bass ? ScoreFont::FClef : ScoreFont::GClef,
         x, layout.StepToY(bass ? 6 : 2));
      x += layout.Spaces(3.4);

      const int count = ScoreTheory::KeyAccidentalCount(track.GetKey());
      for (int i = 0; i < std::abs(count) && i < 7; ++i) {
         const int *steps = count > 0 ? (bass ? BassSharps : TrebleSharps)
                                       : (bass ? BassFlats : TrebleFlats);
         ScoreFont::Draw(dc,
            count > 0 ? ScoreFont::AccidentalSharp : ScoreFont::AccidentalFlat,
            x, layout.StepToY(steps[i]));
         x += layout.Spaces(1.1);
      }
      x += layout.Spaces(0.6);

      auto digits = [](int n) {
         wxString text;
         for (wxChar c : wxString::Format("%d", n))
            text += ScoreFont::Text(ScoreFont::TimeSig0 + (c - '0'));
         return text;
      };
      ScoreFont::Draw(dc, digits(timeSignature.numerator), x, layout.StepToY(6));
      ScoreFont::Draw(dc, digits(timeSignature.denominator), x, layout.StepToY(2));
   }
   CommonChannelView::Draw(context, rect, iPass);
}
