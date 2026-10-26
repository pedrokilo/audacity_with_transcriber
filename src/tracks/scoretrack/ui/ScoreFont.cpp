/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreFont.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace ScoreFont {
bool Available()
{
   static const bool available = [] {
      // The font is installed next to the program
      wxFileName executable(wxStandardPaths::Get().GetExecutablePath());
      wxFileName path(executable.GetPath(), wxT("Leland.otf"));
      if (!path.FileExists())
         return false;
      return wxFont::AddPrivateFont(path.GetFullPath());
   }();
   return available;
}

wxFont Get(int pixels)
{
   Available();
   wxFontInfo info{ wxSize(0, pixels) };
   info.FaceName("Leland");
   return wxFont(info);
}

wxString Text(int glyph)
{
   return wxString(wxUniChar(static_cast<wxUint32>(glyph)));
}

void Draw(wxDC &dc, const wxString &glyphs, int x, int baselineY)
{
   // DrawText positions the top of the text; the glyph origin is the baseline
   wxCoord width = 0, height = 0, descent = 0, leading = 0;
   dc.GetTextExtent(glyphs, &width, &height, &descent, &leading);
   dc.DrawText(glyphs, x, baselineY - (height - descent));
}

void Draw(wxDC &dc, int glyph, int x, int baselineY)
{
   Draw(dc, Text(glyph), x, baselineY);
}
}
