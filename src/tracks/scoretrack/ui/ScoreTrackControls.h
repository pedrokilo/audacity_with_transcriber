/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_TRACK_CONTROLS__
#define __AUDACITY_SCORE_TRACK_CONTROLS__

#include "../../ui/CommonTrackControls.h" // to inherit

class ScoreTrackControls final : public CommonTrackControls
{
   ScoreTrackControls(const ScoreTrackControls &) = delete;
   ScoreTrackControls &operator=(const ScoreTrackControls &) = delete;

public:
   explicit ScoreTrackControls(std::shared_ptr<Track> pTrack)
      : CommonTrackControls(pTrack) {}
   ~ScoreTrackControls() override;

   std::vector<UIHandlePtr> HitTest(
      const TrackPanelMouseState &state,
      const AudacityProject *pProject) override;

   PopupMenuTable *GetMenuExtension(Track *pTrack) override;
};

#endif
