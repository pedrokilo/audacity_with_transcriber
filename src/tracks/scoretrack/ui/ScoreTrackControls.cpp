/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrackControls.h"

#include "ScoreTrack.h"
#include "../../ui/ChannelView.h"
#include "../../../HitTestResult.h"
#include "../../../widgets/PopupMenuTable.h"

ScoreTrackControls::~ScoreTrackControls()
{
}

std::vector<UIHandlePtr> ScoreTrackControls::HitTest(
   const TrackPanelMouseState &state, const AudacityProject *pProject)
{
   return CommonTrackControls::HitTest(state, pProject);
}

PopupMenuTable *ScoreTrackControls::GetMenuExtension(Track *)
{
   // No score-specific entries in the track menu yet.
   return nullptr;
}

using DoGetScoreTrackControls = DoGetControls::Override<ScoreTrack>;
DEFINE_ATTACHED_VIRTUAL_OVERRIDE(DoGetScoreTrackControls) {
   return [](ScoreTrack &track) {
      return std::make_shared<ScoreTrackControls>(track.SharedPointer());
   };
}

using GetDefaultScoreTrackHeight = GetDefaultTrackHeight::Override<ScoreTrack>;
DEFINE_ATTACHED_VIRTUAL_OVERRIDE(GetDefaultScoreTrackHeight) {
   return [](ScoreTrack &) {
      return 180;
   };
}
