/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_TRACK_VRULER_CONTROLS__
#define __AUDACITY_SCORE_TRACK_VRULER_CONTROLS__

#include "../../ui/ChannelVRulerControls.h"

// Present by analogy with other track types; a score has no vertical ruler.
class ScoreTrackVRulerControls final : public ChannelVRulerControls
{
   ScoreTrackVRulerControls(const ScoreTrackVRulerControls &) = delete;
   ScoreTrackVRulerControls &operator=(const ScoreTrackVRulerControls &) = delete;

public:
   explicit ScoreTrackVRulerControls(
      const std::shared_ptr<ChannelView> &pChannelView)
      : ChannelVRulerControls{ pChannelView } {}
   ~ScoreTrackVRulerControls() override;

private:
   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass) override;

   // ChannelVRulerControls implementation
   void UpdateRuler(const wxRect &rect) override;
};

#endif
