/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrack.h"
#include "TempoChange.h"

// When the project tempo changes, the score follows the audio: everything in
// time is scaled, as for the audio clips and the labels.
using OnScoreTrackProjectTempoChange =
   OnProjectTempoChange::Override<ScoreTrack>;
DEFINE_ATTACHED_VIRTUAL_OVERRIDE(OnScoreTrackProjectTempoChange)
{
   return [](ScoreTrack &track, const std::optional<double> &oldTempo,
             double newTempo) {
      if (!oldTempo.has_value() || newTempo <= 0.0)
         return;
      track.ScaleTime(*oldTempo / newTempo);
   };
}
