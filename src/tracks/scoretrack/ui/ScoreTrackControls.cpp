/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrackControls.h"

#include "ScoreGainSliderHandle.h"
#include "ScoreLayout.h"
#include "ScoreTrack.h"

#include "../../playabletrack/ui/PlayableTrackButtonHandles.h"
#include "../../ui/ChannelView.h"
#include "../../ui/CommonTrackInfo.h"
#include "../../../HitTestResult.h"
#include "../../../ProjectWindows.h"
#include "../../../RefreshCode.h"
#include "../../../TrackArtist.h"
#include "../../../TrackPanel.h"
#include "../../../TrackPanelDrawingContext.h"
#include "../../../TrackPanelMouseEvent.h"
#include "../../../widgets/ASlider.h"
#include "../../../widgets/PopupMenuTable.h"
#include "ProjectHistory.h"
#include "Theme.h"
#include "ViewInfo.h"

#include <mutex>

using TCPLine = TrackInfo::TCPLine;

ScoreTrackControls::~ScoreTrackControls()
{
}

std::vector<UIHandlePtr> ScoreTrackControls::HitTest(
   const TrackPanelMouseState &st, const AudacityProject *pProject)
{
   // Hits are mutually exclusive, results single
   std::vector<UIHandlePtr> results;
   const wxMouseState &state = st.state;
   const wxRect &rect = st.rect;
   if (state.ButtonIsDown(wxMOUSE_BTN_ANY)) {
      auto track = std::static_pointer_cast<ScoreTrack>(FindTrack());
      auto result = [&]{
         UIHandlePtr result;
         if (NULL != (result = MuteButtonHandle::HitTest(
            mMuteHandle, state, rect, pProject, track)))
            return result;
         if (NULL != (result = SoloButtonHandle::HitTest(
            mSoloHandle, state, rect, pProject, track)))
            return result;
         if (NULL != (result = ScoreGainSliderHandle::HitTest(
            mGainHandle, state, rect, track)))
            return result;
         return result;
      }();
      if (result) {
         results.push_back(result);
         return results;
      }
   }

   return PlayableTrackControls::HitTest(st, pProject);
}

PopupMenuTable *ScoreTrackControls::GetMenuExtension(Track *)
{
   // Nothing score-specific in the menu of the track panel
   return nullptr;
}

// ---------------------------------------------------------------------------
// Volume slider
// ---------------------------------------------------------------------------
namespace {
void GainSliderDrawFunction(
   TrackPanelDrawingContext &context, const wxRect &rect, const Track *pTrack)
{
   auto dc = &context.dc;
   auto target = dynamic_cast<ScoreGainSliderHandle *>(context.target.get());
   const bool hit = target && target->GetTrack().get() == pTrack;
   const bool captured = hit && target->IsDragging();

   const auto artist = TrackArtist::Get(context);
   auto pParent = FindProjectFrame(artist->parent->GetProject());

   wxRect sliderRect = rect;
   CommonTrackInfo::GetSliderHorizontalBounds(rect, sliderRect);
   auto track = static_cast<const ScoreTrack *>(pTrack);
   ScoreTrackControls::GainSlider(sliderRect, track, captured, pParent)
      ->OnPaint(*dc, hit);
}
}

static const struct ScoreTrackTCPLines
   : TCPLines { ScoreTrackTCPLines() {
   *static_cast<TCPLines *>(this) =
      PlayableTrackControls::StaticNoteTCPLines();
   insert( end(), {
      { TCPLine::kItemVolume, kTrackInfoSliderHeight, kTrackInfoSliderExtra,
        GainSliderDrawFunction },
   } );
} } scoreTrackTCPLines;

void ScoreTrackControls::GetGainRect(const wxRect &rect_, wxRect &dest)
{
   const auto rect = wxRect(rect_).Deflate(CommonTrackInfo::Margin);
   CommonTrackInfo::GetSliderHorizontalBounds(rect, dest);
   const auto results = CalcItemY(scoreTrackTCPLines, TCPLine::kItemVolume);
   dest.y = rect.y + results.first;
   dest.height = results.second;
}

unsigned ScoreTrackControls::DefaultScoreTrackHeight()
{
   return std::max<unsigned>(
      CommonTrackInfo::DefaultTrackHeight(scoreTrackTCPLines),
      ScoreLayout::DefaultTrackHeight);
}

const TCPLines &ScoreTrackControls::GetTCPLines() const
{
   return scoreTrackTCPLines;
}

namespace {
std::unique_ptr<LWSlider> gGainCaptured, gGain;
}

LWSlider *ScoreTrackControls::GainSlider(
   const wxRect &sliderRect, const ScoreTrack *t, bool captured,
   wxWindow *pParent)
{
   static std::once_flag flag;
   std::call_once(flag, []{ ReCreateGainSlider({}); });
   static auto subscription = theTheme.Subscribe(ReCreateGainSlider);

   wxPoint pos = sliderRect.GetPosition();
   float gain = t ? t->GetGain() : 1.0f;

   gGain->Move(pos);
   gGain->Set(gain);
   gGainCaptured->Move(pos);
   gGainCaptured->Set(gain);

   auto slider = (captured ? gGainCaptured : gGain).get();
   slider->SetParent(pParent);
   return slider;
}

void ScoreTrackControls::ReCreateGainSlider(ThemeChangeMessage message)
{
   if (message.appearance)
      return;

   const auto sliderRect =
      wxRect(0, 0, kTrackInfoSliderWidth, kTrackInfoSliderHeight);

   /* i18n-hint: Title of the Volume slider, used to adjust the volume */
   gGain = std::make_unique<LWSlider>(nullptr, XO("Volume"),
      wxPoint(sliderRect.x, sliderRect.y),
      wxSize(sliderRect.width, sliderRect.height),
      DB_SLIDER);
   gGain->SetDefaultValue(1.0);
   gGainCaptured = std::make_unique<LWSlider>(nullptr, XO("Volume"),
      wxPoint(sliderRect.x, sliderRect.y),
      wxSize(sliderRect.width, sliderRect.height),
      DB_SLIDER);
   gGainCaptured->SetDefaultValue(1.0);
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
      return ScoreTrackControls::DefaultScoreTrackHeight();
   };
}
