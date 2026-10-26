/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_FONT__
#define __AUDACITY_SCORE_FONT__

#include <wx/dc.h>
#include <wx/font.h>
#include <wx/string.h>

//! The music font used to engrave the score: Leland, from MuseScore (SIL Open
//! Font License). Glyphs follow the SMuFL standard: the font size is four
//! staff spaces, and each glyph is placed by its origin, which sits on the
//! baseline.
namespace ScoreFont {
//! SMuFL code points
enum Glyph : int {
   TimeSig0 = 0xE080,
   AugmentationDot = 0xE1E7,
   GClef = 0xE050,
   FClef = 0xE062,
   NoteheadWhole = 0xE0A2,
   NoteheadHalf = 0xE0A3,
   NoteheadBlack = 0xE0A4,
   Flag8thUp = 0xE240,
   Flag8thDown = 0xE241,
   Flag16thUp = 0xE242,
   Flag16thDown = 0xE243,
   Flag32ndUp = 0xE244,
   Flag32ndDown = 0xE245,
   AccidentalFlat = 0xE260,
   AccidentalNatural = 0xE261,
   AccidentalSharp = 0xE262,
   RestWhole = 0xE4E3,
   RestHalf = 0xE4E4,
   RestQuarter = 0xE4E5,
   Rest8th = 0xE4E6,
   Rest16th = 0xE4E7,
   Rest32nd = 0xE4E8,
   // Notes with a stem, as shown in metronome marks: used by the palette
   NoteWhole = 0xECA2,
   NoteHalfUp = 0xECA3,
   NoteQuarterUp = 0xECA5,
   Note8thUp = 0xECA7,
   Note16thUp = 0xECA9,
   Note32ndUp = 0xECAB,
};

//! Registers the font on first use; false if it could not be found
bool Available();

//! The font at a size given in pixels (four staff spaces for engraving)
wxFont Get(int pixels);

//! Draws a glyph with its origin at (x, baselineY). The font must already be
//! selected in the dc.
void Draw(wxDC &dc, int glyph, int x, int baselineY);

//! Draws a string of glyphs
void Draw(wxDC &dc, const wxString &glyphs, int x, int baselineY);

//! A string made of one glyph
wxString Text(int glyph);
}

#endif
