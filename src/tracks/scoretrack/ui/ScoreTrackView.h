/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_TRACK_VIEW__
#define __AUDACITY_SCORE_TRACK_VIEW__

#include "../../ui/CommonChannelView.h"

//! Prototype view of a ScoreTrack: draws an empty staff across the whole track.
class ScoreTrackView final : public CommonChannelView
{
   ScoreTrackView(const ScoreTrackView &) = delete;
   ScoreTrackView &operator=(const ScoreTrackView &) = delete;

public:
   explicit ScoreTrackView(const std::shared_ptr<Channel> &pChannel);
   ~ScoreTrackView() override;

   std::shared_ptr<ChannelVRulerControls> DoGetVRulerControls() override;

private:
   std::vector<UIHandlePtr> DetailedHitTest(
      const TrackPanelMouseState &state,
      const AudacityProject *pProject, int currentTool, bool bMultiTool)
      override;

   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass) override;
};

#endif
