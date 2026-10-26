/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreGainSliderHandle.h"

#include "ScoreTrack.h"
#include "ScoreTrackControls.h"

#include "../../ui/CommonTrackInfo.h"
#include "../../../RefreshCode.h"
#include "../../../TrackPanel.h"
#include "../../../widgets/ASlider.h"
#include "Decibels.h"
#include "ProjectHistory.h"
#include "ViewInfo.h"

ScoreGainSliderHandle::ScoreGainSliderHandle(
   SliderFn sliderFn, const wxRect &rect,
   const std::shared_ptr<Track> &pTrack)
   : SliderHandle{ sliderFn, rect, pTrack }
{}

ScoreGainSliderHandle::~ScoreGainSliderHandle()
{
}

std::shared_ptr<ScoreTrack> ScoreGainSliderHandle::GetScoreTrack() const
{
   return std::static_pointer_cast<ScoreTrack>(mpTrack.lock());
}

float ScoreGainSliderHandle::GetValue()
{
   if (auto pTrack = GetScoreTrack())
      return pTrack->GetGain();
   return 1.0f;
}

UIHandle::Result ScoreGainSliderHandle::SetValue(
   AudacityProject *, float newValue)
{
   if (auto pTrack = GetScoreTrack())
      pTrack->SetGain(newValue);
   return RefreshCode::RefreshNone;
}

UIHandle::Result ScoreGainSliderHandle::CommitChanges(
   const wxMouseEvent &, AudacityProject *pProject)
{
   ProjectHistory::Get(*pProject)
      .PushState(XO("Moved volume slider"), XO("Volume"), UndoPush::CONSOLIDATE);
   return RefreshCode::RefreshCell;
}

TranslatableString ScoreGainSliderHandle::Tip(
   const wxMouseState &, AudacityProject &) const
{
   float value = 1.0f;
   if (auto pTrack = GetScoreTrack())
      value = pTrack->GetGain();
   /* i18n-hint dB abbreviates decibels */
   const auto val = XO("%+.1f dB").Format(LINEAR_TO_DB(value));
   /* i18n-hint: An item name followed by a value, with appropriate separating punctuation */
   return XO("%s: %s").Format(XO("Volume"), val);
}

UIHandlePtr ScoreGainSliderHandle::HitTest(
   std::weak_ptr<ScoreGainSliderHandle> &holder,
   const wxMouseState &state, const wxRect &rect,
   const std::shared_ptr<Track> &pTrack)
{
   if (!state.ButtonIsDown(wxMOUSE_BTN_LEFT))
      return {};

   wxRect sliderRect;
   ScoreTrackControls::GetGainRect(rect, sliderRect);
   if (CommonTrackInfo::HideTopItem(rect, sliderRect))
      return {};
   if (sliderRect.Contains(state.m_x, state.m_y)) {
      auto sliderFn =
      [](AudacityProject *pProject, const wxRect &sliderRect, Track *pTrack) {
         return ScoreTrackControls::GainSlider(
            sliderRect, static_cast<ScoreTrack *>(pTrack), true,
            &TrackPanel::Get(*pProject));
      };
      auto result = std::make_shared<ScoreGainSliderHandle>(
         sliderFn, sliderRect, pTrack);
      return AssignUIHandlePtr(holder, result);
   }
   return {};
}
