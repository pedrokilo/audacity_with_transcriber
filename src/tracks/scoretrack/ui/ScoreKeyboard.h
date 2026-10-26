/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_KEYBOARD__
#define __AUDACITY_SCORE_KEYBOARD__

#include <wx/minifram.h>
#include <wx/weakref.h>

#include "ScoreTrack.h"

class wxChoice;
class wxToggleButton;
class wxCommandEvent;

//! What a click on the staff will enter
struct ScoreInput {
   ScoreDuration duration{ ScoreDuration::Quarter };
   int dots{ 0 };
   ScoreAccidental accidental{ ScoreAccidental::None };
   bool rest{ false };
   bool tie{ false };
   bool barLine{ false };   //!< a click draws a bar line instead of a note
};

//! Floating palette shown while a score track is selected: note value, dot,
//! accidental, rest and tie, plus the clef, key signature and time signature
//! of the selected score tracks.
class ScoreKeyboard final : public wxMiniFrame
{
public:
   //! Show or hide the palette according to the selection in the active
   //! project; also refreshes the clef, key and time choices.
   static void Update();

   //! Current input settings (defaults when the palette was never shown)
   static ScoreInput Input();

private:
   explicit ScoreKeyboard(wxWindow *parent);
   ~ScoreKeyboard() override;

   void SyncFromTrack(const ScoreTrack &track);
   void OnSettingChoice(wxCommandEvent &);
   void OnDuration(int index);
   void OnToggles();
   void OnBarsFromSplits();

   wxChoice *mClef{};
   wxChoice *mKey{};
   wxChoice *mTime{};
   wxToggleButton *mDuration[static_cast<int>(ScoreDuration::Count)]{};
   wxToggleButton *mDot{};
   wxToggleButton *mRest{};
   wxToggleButton *mTie{};
   wxToggleButton *mBar{};
   wxToggleButton *mAcc[3]{};   // natural, sharp, flat
   bool mSyncing{ false };
   ScoreInput mInput;

};

#endif
