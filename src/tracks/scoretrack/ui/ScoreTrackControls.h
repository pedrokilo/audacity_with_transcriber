/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_TRACK_CONTROLS__
#define __AUDACITY_SCORE_TRACK_CONTROLS__

#include "../../playabletrack/ui/PlayableTrackControls.h" // to inherit

class LWSlider;
class ScoreTrack;
class MuteButtonHandle;
class SoloButtonHandle;
class ScoreGainSliderHandle;

//! Track controls of a score: title, mute, solo and a volume slider
class ScoreTrackControls final : public PlayableTrackControls
{
   ScoreTrackControls(const ScoreTrackControls &) = delete;
   ScoreTrackControls &operator=(const ScoreTrackControls &) = delete;

   std::weak_ptr<MuteButtonHandle> mMuteHandle;
   std::weak_ptr<SoloButtonHandle> mSoloHandle;
   std::weak_ptr<ScoreGainSliderHandle> mGainHandle;

public:
   explicit ScoreTrackControls(std::shared_ptr<Track> pTrack)
      : PlayableTrackControls(pTrack) {}
   ~ScoreTrackControls() override;

   std::vector<UIHandlePtr> HitTest(
      const TrackPanelMouseState &state,
      const AudacityProject *pProject) override;

   PopupMenuTable *GetMenuExtension(Track *pTrack) override;
   const TCPLines &GetTCPLines() const override;

   static unsigned DefaultScoreTrackHeight();
   static void GetGainRect(const wxRect &rect, wxRect &dest);

   static LWSlider *GainSlider(
      const wxRect &sliderRect, const ScoreTrack *t, bool captured,
      wxWindow *pParent);

private:
   static void ReCreateGainSlider(struct ThemeChangeMessage);
};

#endif
