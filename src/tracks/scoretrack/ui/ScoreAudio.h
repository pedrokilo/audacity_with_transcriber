/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_AUDIO__
#define __AUDACITY_SCORE_AUDIO__

#include <vector>

class AudacityProject;

//! Start and end times of every audio clip of the project, sorted: the places
//! where the audio was split.
std::vector<double> ScoreAudioSplitTimes(AudacityProject &project);

#endif
