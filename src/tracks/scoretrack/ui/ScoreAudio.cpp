/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreAudio.h"

#include "Project.h"
#include "Track.h"
#include "WaveClip.h"
#include "WaveTrack.h"

#include <algorithm>

std::vector<double> ScoreAudioSplitTimes(AudacityProject &project)
{
   std::vector<double> times;
   for (auto track : TrackList::Get(project).Any<const WaveTrack>()) {
      for (const auto &pInterval : track->Intervals()) {
         times.push_back(pInterval->Start());
         times.push_back(pInterval->End());
      }
   }
   std::sort(times.begin(), times.end());
   times.erase(std::unique(times.begin(), times.end(),
      [](double a, double b) { return std::abs(a - b) < 1e-4; }), times.end());
   return times;
}
