/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrackView.h"

#include "ScoreTrack.h"
#include "ScoreTrackVRulerControls.h"

#include "AColor.h"
#include "AllThemeResources.h"
#include "PendingTracks.h"
#include "SyncLock.h"
#include "Theme.h"
#include "ViewInfo.h"
#include "../../../TrackArt.h"
#include "../../../TrackArtist.h"
#include "../../../TrackPanelDrawingContext.h"

#include <algorithm>

#include <wx/dc.h>

namespace {
// The staff fills this fraction of the track height, so it grows when the
// track is made taller.
constexpr double StaffFillFraction = 0.7;
}

ScoreTrackView::ScoreTrackView(const std::shared_ptr<Channel> &pChannel)
   : CommonChannelView{ pChannel }
{
}

ScoreTrackView::~ScoreTrackView()
{
}

std::vector<UIHandlePtr> ScoreTrackView::DetailedHitTest(
   const TrackPanelMouseState &, const AudacityProject *, int, bool)
{
   return {};
}

using DoGetScoreTrackView = DoGetView::Override<ScoreTrack>;
DEFINE_ATTACHED_VIRTUAL_OVERRIDE(DoGetScoreTrackView) {
   return [](ScoreTrack &track, size_t) {
      return std::make_shared<ScoreTrackView>(
         track.SharedPointer<ScoreTrack>());
   };
}

std::shared_ptr<ChannelVRulerControls> ScoreTrackView::DoGetVRulerControls()
{
   return std::make_shared<ScoreTrackVRulerControls>(shared_from_this());
}

void ScoreTrackView::Draw(
   TrackPanelDrawingContext &context, const wxRect &rect, unsigned iPass)
{
   if (iPass == TrackArtist::PassTracks && FindChannel()) {
      auto &dc = context.dc;
      const auto artist = TrackArtist::Get(context);
      const auto &zoomInfo = *artist->pZoomInfo;
      const auto &pendingTracks = *artist->pPendingTracks;

      const auto &track = static_cast<const ScoreTrack &>(
         pendingTracks.SubstitutePendingChangedChannel(*FindChannel()));

      TrackArt::DrawBackgroundWithSelection(context, rect, track,
         AColor::labelSelectedBrush, AColor::labelUnselectedBrush,
         SyncLock::IsSelectedOrSyncLockSelected(track));

      const int nLines = track.GetStaffLines();
      const int intervals = std::max(1, nLines - 1);
      // Distance between two staff lines, in pixels
      const int staffSpace =
         std::max(4, static_cast<int>(rect.height * StaffFillFraction / intervals));
      const int staffHeight = intervals * staffSpace;
      const int top = rect.y + (rect.height - staffHeight) / 2;

      dc.SetPen(wxPen(theTheme.Colour(clrTrackPanelText)));
      for (int i = 0; i < nLines; ++i) {
         const int y = top + i * staffSpace;
         dc.DrawLine(rect.x, y, rect.x + rect.width - 1, y);
      }

      // Bar line at time 0: follows zoom and scroll, proving time sync.
      const int xOrigin = zoomInfo.TimeToPosition(0.0, rect.x);
      if (xOrigin >= rect.x && xOrigin < rect.x + rect.width)
         dc.DrawLine(xOrigin, top, xOrigin, top + staffHeight);
   }
   CommonChannelView::Draw(context, rect, iPass);
}
