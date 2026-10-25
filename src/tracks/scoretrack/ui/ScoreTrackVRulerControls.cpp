/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrackVRulerControls.h"

#include "AColor.h"
#include "../../../TrackArtist.h"
#include "../../../TrackPanelDrawingContext.h"

ScoreTrackVRulerControls::~ScoreTrackVRulerControls()
{
}

void ScoreTrackVRulerControls::Draw(
   TrackPanelDrawingContext &context, const wxRect &rect_, unsigned iPass)
{
   ChannelVRulerControls::Draw(context, rect_, iPass);
}

void ScoreTrackVRulerControls::UpdateRuler(const wxRect &)
{
   // A score has no vertical ruler.
}
