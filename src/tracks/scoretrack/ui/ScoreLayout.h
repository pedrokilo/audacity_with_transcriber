/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_LAYOUT__
#define __AUDACITY_SCORE_LAYOUT__

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <wx/gdicmn.h>

#include "ScoreTrack.h"

//! Vertical geometry of a staff inside a track cell.
//!
//! A "step" is a half staff space counted from the bottom line: step 0 is the
//! bottom line, step 1 the first space, step 2 the second line... Notes can sit
//! up to MaxLedgerLines ledger lines above and below the staff.
struct ScoreLayout {
   static constexpr int Space = 10;           //!< pixels between two lines
   static constexpr int MaxLedgerLines = 3;   //!< above and below the staff
   //! Default height of a score track: the staff, the ledger lines, margins
   static constexpr int DefaultTrackHeight =
      (4 + 2 * MaxLedgerLines) * Space + 6 * Space;

   int space{ Space };
   int lines{ 5 };
   int bottom{ 0 };  //!< y of the bottom staff line

   static ScoreLayout For(const wxRect &rect, int lines)
   {
      ScoreLayout l;
      l.lines = std::max(1, lines);
      const int staffHeight = (l.lines - 1) * l.space;
      l.bottom = rect.y + (rect.height + staffHeight) / 2;
      return l;
   }

   int TopLineStep() const { return (lines - 1) * 2; }
   int MinStep() const { return -2 * MaxLedgerLines - 1; }
   int MaxStep() const { return TopLineStep() + 2 * MaxLedgerLines + 1; }

   int StepToY(int step) const { return bottom - (step * space) / 2; }
   int YToStep(int y) const
   {
      const double step = (bottom - y) * 2.0 / space;
      return std::clamp(static_cast<int>(std::lround(step)), MinStep(), MaxStep());
   }
   int StaffTop() const { return StepToY(TopLineStep()); }
   int StaffCenter() const { return (StaffTop() + bottom) / 2; }
   //! Pixels for a length given in staff spaces
   int Spaces(double n) const { return static_cast<int>(std::lround(n * space)); }
};

//! Width of the clef, key signature and time signature area
inline int ScoreHeaderWidth(const ScoreTrack &track)
{
   const int accidentals = std::abs(ScoreTheory::KeyAccidentalCount(track.GetKey()));
   const double spaces = 0.8 + 2.8 + 0.6 + accidentals * 1.1 + 0.6 + 1.9 + 0.8;
   return static_cast<int>(std::lround(spaces * ScoreLayout::Space));
}

#endif
