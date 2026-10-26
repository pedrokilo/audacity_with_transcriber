/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>
#include <vector>

#include "AudioIOSequences.h"

class ScoreTrack;

//! Lets the mixer play a score track like any other track: it synthesises the
//! notes on demand, so looping, seeking, playback speed and latency work as
//! they do for audio.
//!
//! It works on a snapshot of the notes taken when playback starts, so editing
//! the score while it sounds cannot disturb the audio thread.
class SCORE_TRACK_API ScoreSequence final : public PlayableSequence
{
public:
   static constexpr double SampleRate = 44100.0;

   static std::shared_ptr<ScoreSequence> Create(
      const std::shared_ptr<const ScoreTrack> &pTrack);

   ~ScoreSequence() override;

   // AudioGraph::Channel
   AudioGraph::ChannelType GetChannelType() const override;

   // PlayableSequence
   const ChannelGroup *FindChannelGroup() const override;
   bool GetSolo() const override;
   bool GetMute() const override;

   // WideSampleSequence
   size_t NChannels() const override;
   float GetChannelVolume(int channel) const override;
   bool DoGet(
      size_t iChannel, size_t nBuffers, const samplePtr buffers[],
      sampleFormat format, sampleCount start, size_t len, bool backward,
      fillFormat fill = FillFormat::fillZero, bool mayThrow = true,
      sampleCount *pNumWithinClips = nullptr) const override;
   double GetStartTime() const override;
   double GetEndTime() const override;
   double GetRate() const override;
   sampleFormat WidestEffectiveFormat() const override;
   bool HasTrivialEnvelope() const override;
   void GetEnvelopeValues(
      double *buffer, size_t bufferLen, double t0, bool backwards) const override;

   //! One tone of the snapshot
   struct Voice {
      double start;
      double end;
      double frequency;
   };

private:
   explicit ScoreSequence(const std::shared_ptr<const ScoreTrack> &pTrack);

   const std::shared_ptr<const ScoreTrack> mpTrack;
   std::vector<Voice> mVoices;   //!< sorted by start
   double mStart{ 0.0 };
   double mEnd{ 0.0 };
};
