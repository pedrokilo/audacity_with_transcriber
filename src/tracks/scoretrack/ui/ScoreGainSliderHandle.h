/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_GAIN_SLIDER_HANDLE__
#define __AUDACITY_SCORE_GAIN_SLIDER_HANDLE__

#include "../../ui/SliderHandle.h"

class ScoreTrack;
class wxMouseState;

//! Moves the volume slider of a score track
class ScoreGainSliderHandle final : public SliderHandle
{
   ScoreGainSliderHandle(const ScoreGainSliderHandle &) = delete;
   std::shared_ptr<ScoreTrack> GetScoreTrack() const;

public:
   explicit ScoreGainSliderHandle(
      SliderFn sliderFn, const wxRect &rect,
      const std::shared_ptr<Track> &pTrack);
   ScoreGainSliderHandle &operator=(const ScoreGainSliderHandle &) = default;
   virtual ~ScoreGainSliderHandle();

protected:
   float GetValue() override;
   Result SetValue(AudacityProject *pProject, float newValue) override;
   Result CommitChanges(
      const wxMouseEvent &event, AudacityProject *pProject) override;
   TranslatableString Tip(
      const wxMouseState &state, AudacityProject &) const override;
   bool StopsOnKeystroke() override { return true; }

public:
   static UIHandlePtr HitTest(
      std::weak_ptr<ScoreGainSliderHandle> &holder,
      const wxMouseState &state, const wxRect &rect,
      const std::shared_ptr<Track> &pTrack);
};

#endif
