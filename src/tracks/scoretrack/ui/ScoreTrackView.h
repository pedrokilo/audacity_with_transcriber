/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_TRACK_VIEW__
#define __AUDACITY_SCORE_TRACK_VIEW__

#include "../../ui/CommonChannelView.h"
#include "Observer.h"

class ScoreNoteHandle;
class ScoreTrack;

//! View of a ScoreTrack: the staff with its ledger lines, the clef, key and
//! time signatures, bar lines and the notes, aligned with the project timeline.
class ScoreTrackView final : public CommonChannelView
{
   ScoreTrackView(const ScoreTrackView &) = delete;
   ScoreTrackView &operator=(const ScoreTrackView &) = delete;

   void Reparent(const std::shared_ptr<Track> &parent, size_t iChannel)
      override;

public:
   explicit ScoreTrackView(const std::shared_ptr<Channel> &pChannel);
   ~ScoreTrackView() override;

   std::shared_ptr<ChannelVRulerControls> DoGetVRulerControls() override;

private:
   void BindTo(ScoreTrack *pTrack);

   std::vector<UIHandlePtr> DetailedHitTest(
      const TrackPanelMouseState &state,
      const AudacityProject *pProject, int currentTool, bool bMultiTool)
      override;

   //! The menu of a right click on the staff: the cursor goes where the click
   //! was, and it offers to split or join the bars there
   std::vector<MenuItem> GetMenuItems(
      const wxRect &rect, const wxPoint *pPosition, AudacityProject *pProject)
      override;

   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass) override;

   std::weak_ptr<ScoreNoteHandle> mNoteHandle;
   Observer::Subscription mSubscription;
};

#endif
