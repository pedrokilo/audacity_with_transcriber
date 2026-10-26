/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreSound.h"

#include "ScoreTrack.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace {
constexpr int SampleRate = 44100;
// Never synthesise more than this much sound in one go
constexpr double MaxSeconds = 30.0 * 60.0;

// Wraps 16 bit mono samples in a WAV file image
std::vector<char> MakeWav(const std::vector<int16_t> &samples)
{
   const uint32_t dataBytes = static_cast<uint32_t>(samples.size()) * 2;
   std::vector<char> wav(44 + dataBytes);

   auto put32 = [&](size_t pos, uint32_t v) { std::memcpy(&wav[pos], &v, 4); };
   auto put16 = [&](size_t pos, uint16_t v) { std::memcpy(&wav[pos], &v, 2); };

   std::memcpy(&wav[0], "RIFF", 4);
   put32(4, 36 + dataBytes);
   std::memcpy(&wav[8], "WAVEfmt ", 8);
   put32(16, 16);
   put16(20, 1);                 // PCM
   put16(22, 1);                 // mono
   put32(24, SampleRate);
   put32(28, SampleRate * 2);
   put16(32, 2);
   put16(34, 16);
   std::memcpy(&wav[36], "data", 4);
   put32(40, dataBytes);
   if (dataBytes)
      std::memcpy(&wav[44], samples.data(), dataBytes);
   return wav;
}

// PlaySound with SND_ASYNC needs the buffer to outlive the call
std::vector<char> &CurrentWave()
{
   static std::vector<char> wave;
   return wave;
}
}

namespace ScoreSound {
void Stop()
{
#ifdef _WIN32
   PlaySoundW(nullptr, nullptr, 0);
#endif
}

void PlayVoices(const std::vector<Voice> &voices, double origin)
{
#ifdef _WIN32
   // Length of the mix
   double end = 0.0;
   for (const auto &v : voices)
      end = std::max(end, v.end - origin);
   end = std::min(end + 0.25, MaxSeconds);
   if (voices.empty() || end <= 0.0) {
      Stop();
      return;
   }

   std::vector<float> mix(static_cast<size_t>(end * SampleRate) + 1, 0.0f);
   const double twoPi = 6.283185307179586;

   for (const auto &v : voices) {
      const double seconds = std::max(0.05, v.end - v.start);
      const double begin = v.start - origin;
      const double attack = 0.01;
      const double release = std::min(0.12, seconds / 2);
      const auto first = static_cast<long long>(std::max(0.0, begin) * SampleRate);
      const auto count = static_cast<long long>((seconds + 0.1) * SampleRate);
      for (long long i = 0; i < count; ++i) {
         const long long index = first + i;
         if (index < 0 || index >= static_cast<long long>(mix.size()))
            break;
         const double t = static_cast<double>(i) / SampleRate;
         // A note that began before the origin is heard from where it is
         double env = 1.0;
         if (t < attack)
            env = t / attack;
         else if (t > seconds)
            env = std::max(0.0, 1.0 - (t - seconds) / 0.1);
         else if (t > seconds - release)
            env = std::max(0.0, (seconds - t) / release * 0.6 + 0.4);
         env *= std::exp(-0.9 * t);
         const double phase = twoPi * v.frequency * t;
         mix[static_cast<size_t>(index)] += static_cast<float>(
            env * (0.6 * std::sin(phase) + 0.25 * std::sin(2 * phase)
               + 0.1 * std::sin(3 * phase)));
      }
   }

   // Normalise only if chords add up past full scale
   float peak = 0.0f;
   for (float s : mix)
      peak = std::max(peak, std::abs(s));
   const float gain = 0.5f * (peak > 0.9f ? 0.9f / peak : 1.0f) * 32767.0f;

   std::vector<int16_t> samples(mix.size());
   for (size_t i = 0; i < mix.size(); ++i)
      samples[i] = static_cast<int16_t>(mix[i] * gain * 2.0f);

   Stop();
   CurrentWave() = MakeWav(samples);
   PlaySoundW(reinterpret_cast<LPCWSTR>(CurrentWave().data()), nullptr,
      SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
   (void)voices;
   (void)origin;
#endif
}

void PlayNote(int midi, double seconds)
{
   seconds = std::clamp(seconds, 0.15, 2.0);
   PlayVoices({ { 0.0, seconds, ScoreTheory::Frequency(midi) } }, 0.0);
}
}
