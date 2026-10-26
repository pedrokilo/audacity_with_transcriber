/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_SOUND__
#define __AUDACITY_SCORE_SOUND__

#include <vector>

namespace ScoreSound {
//! A tone to synthesise: it sounds from start to end (seconds)
struct Voice {
   double start;
   double end;
   double frequency;
};

//! Plays a short tone for a MIDI note (asynchronously, replacing any sound
//! still playing). Used to audition a note when it is entered.
void PlayNote(int midi, double seconds);

//! Synthesises the voices and plays them asynchronously, starting with the
//! sound that begins at @p origin. Replaces any sound still playing.
void PlayVoices(const std::vector<Voice> &voices, double origin);

//! Stops whatever is playing
void Stop();
}

#endif
