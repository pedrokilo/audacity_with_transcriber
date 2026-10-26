/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreSequence.h"

#include "ScoreTrack.h"
#include "SampleFormat.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double TwoPi = 6.283185307179586;
constexpr double Attack = 0.008;      // seconds
constexpr double Tail = 0.12;         // sound after the note is over
constexpr double Gain = 0.4;          // per voice, leaves room for chords
}

std::shared_ptr<ScoreSequence> ScoreSequence::Create(
   const std::shared_ptr<const ScoreTrack> &pTrack)
{
   return std::shared_ptr<ScoreSequence>(new ScoreSequence(pTrack));
}

ScoreSequence::ScoreSequence(const std::shared_ptr<const ScoreTrack> &pTrack)
   : mpTrack{ pTrack }
{
   const auto &notes = mpTrack->GetNotes();
   std::vector<bool> absorbed(notes.size(), false);
   for (size_t i = 0; i < notes.size(); ++i) {
      const auto &note = notes[i];
      if (note.rest || absorbed[i])
         continue;
      const double start = note.time;
      double end = start + note.length;

      // A tied note continues into the next note of the same pitch
      size_t current = i;
      while (notes[current].tie) {
         size_t next = notes.size();
         for (size_t j = current + 1; j < notes.size(); ++j) {
            if (!notes[j].rest && notes[j].dia == note.dia
                && notes[j].alter == note.alter) {
               next = j;
               break;
            }
         }
         if (next == notes.size())
            break;
         absorbed[next] = true;
         end = notes[next].time + notes[next].length;
         current = next;
      }

      mVoices.push_back({ start, end,
         ScoreTheory::Frequency(ScoreTheory::Midi(note.dia, note.alter)) });
   }
   std::stable_sort(mVoices.begin(), mVoices.end(),
      [](const Voice &a, const Voice &b) { return a.start < b.start; });

   if (!mVoices.empty()) {
      mStart = mVoices.front().start;
      for (const auto &v : mVoices)
         mEnd = std::max(mEnd, v.end);
      mEnd += Tail;
   }
}

ScoreSequence::~ScoreSequence() = default;

AudioGraph::ChannelType ScoreSequence::GetChannelType() const
{
   return AudioGraph::MonoChannel;
}

const ChannelGroup *ScoreSequence::FindChannelGroup() const
{
   return mpTrack.get();
}

bool ScoreSequence::GetSolo() const
{
   return mpTrack->GetSolo();
}

bool ScoreSequence::GetMute() const
{
   return mpTrack->GetMute();
}

size_t ScoreSequence::NChannels() const
{
   return 1;
}

float ScoreSequence::GetChannelVolume(int) const
{
   return mpTrack->GetGain();
}

double ScoreSequence::GetStartTime() const
{
   return mStart;
}

double ScoreSequence::GetEndTime() const
{
   return mEnd;
}

double ScoreSequence::GetRate() const
{
   return SampleRate;
}

sampleFormat ScoreSequence::WidestEffectiveFormat() const
{
   return floatSample;
}

bool ScoreSequence::HasTrivialEnvelope() const
{
   return true;
}

void ScoreSequence::GetEnvelopeValues(
   double *buffer, size_t bufferLen, double, bool) const
{
   std::fill(buffer, buffer + bufferLen, 1.0);
}

bool ScoreSequence::DoGet(
   size_t iChannel, size_t nBuffers, const samplePtr buffers[],
   sampleFormat format, sampleCount start, size_t len, bool backward,
   fillFormat, bool, sampleCount *pNumWithinClips) const
{
   if (iChannel != 0 || nBuffers < 1)
      return false;

   std::vector<float> out(len, 0.0f);

   // The time of each sample of the request, in increasing order
   const long long first = start.as_long_long();
   const long long lowest = backward
      ? first - static_cast<long long>(len) : first;
   const double t0 = static_cast<double>(lowest) / SampleRate;
   const double t1 = t0 + static_cast<double>(len) / SampleRate;

   for (const auto &voice : mVoices) {
      if (voice.start > t1)
         break;                       // sorted by start
      if (voice.end + Tail < t0)
         continue;

      const double length = std::max(0.02, voice.end - voice.start);
      const double release = std::min(Tail, length / 2);
      for (size_t i = 0; i < len; ++i) {
         const long long position = lowest + static_cast<long long>(i);
         const double t = static_cast<double>(position) / SampleRate - voice.start;
         if (t < 0.0 || t > length + Tail)
            continue;

         double env = t < Attack ? t / Attack : 1.0;
         if (t > length)
            env *= std::max(0.0, 1.0 - (t - length) / Tail);
         else if (t > length - release)
            env *= 0.6 + 0.4 * (length - t) / release;
         env *= std::exp(-0.9 * t);

         const double phase = TwoPi * voice.frequency * t;
         const double sample = env * (0.6 * std::sin(phase)
            + 0.25 * std::sin(2 * phase) + 0.1 * std::sin(3 * phase));
         out[i] += static_cast<float>(Gain * sample);
      }
   }

   for (auto &s : out)
      s = std::clamp(s, -1.0f, 1.0f);

   // Backward requests want the samples in reverse order
   if (backward)
      std::reverse(out.begin(), out.end());

   CopySamples(reinterpret_cast<constSamplePtr>(out.data()), floatSample,
      buffers[0], format, len);

   if (pNumWithinClips)
      *pNumWithinClips = len;
   return true;
}
