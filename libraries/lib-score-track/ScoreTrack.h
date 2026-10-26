/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <vector>

#include "Observer.h"
#include <atomic>

#include "PlayableTrack.h"

class AudacityProject;

// ---------------------------------------------------------------------------
// Music theory model
//
// The sounding pitch is stored apart from the written position (design rule 2):
// a note stores its absolute diatonic position `dia` (octave * 7 + letter,
// with C = 0 ... B = 6, so middle C is 28) and the `alter` actually applied to
// it. Where it sits on a staff is derived from the clef, so changing the clef
// never changes what is heard.
// ---------------------------------------------------------------------------

enum class ScoreClef : int { Treble = 0, Bass = 1 };

//! Key signatures offered for now (major and minor of C, A, G and F)
enum class ScoreKey : int { C = 0, Cm, A, Am, G, Gm, F, Fm, Count };

//! Written accidental shown next to a note (None: the key signature rules)
enum class ScoreAccidental : int { None = 0, Natural, Sharp, Flat };

//! Note value, as a power of two fraction of a whole note
enum class ScoreDuration : int {
   Whole = 0, Half, Quarter, Eighth, Sixteenth, ThirtySecond, Count };

struct ScoreTimeSignature {
   int numerator{ 4 };
   int denominator{ 4 };
   bool operator==(const ScoreTimeSignature &o) const
   { return numerator == o.numerator && denominator == o.denominator; }
};

struct ScoreNote {
   double time{ 0.0 };           //!< start, in seconds from the project origin
   //! How long it sounds, in seconds. Fixed when the note is entered, so a
   //! later change of the project tempo moves nothing that is already written.
   double length{ 0.5 };
   int dia{ 30 };                //!< absolute diatonic position (see above)
   int alter{ 0 };               //!< semitones applied to the natural letter
   ScoreAccidental accidental{ ScoreAccidental::None };
   ScoreDuration duration{ ScoreDuration::Quarter };
   int dots{ 0 };
   bool rest{ false };
   bool tie{ false };            //!< tied to the next note of the same pitch
};

// Pure helpers (no GUI, no project state)
namespace ScoreTheory {
//! Diatonic position of the bottom staff line
SCORE_TRACK_API int BottomLineDia(ScoreClef clef);
//! MIDI note number of a note
SCORE_TRACK_API int Midi(int dia, int alter);
//! Alteration the key signature applies to a letter (0 = C ... 6 = B)
SCORE_TRACK_API int KeyAlteration(ScoreKey key, int letter);
//! Positive: number of sharps; negative: number of flats
SCORE_TRACK_API int KeyAccidentalCount(ScoreKey key);
//! Length of a note in whole notes (dots included)
SCORE_TRACK_API double LengthInWholeNotes(ScoreDuration d, int dots);
//! Length of a note in seconds at the given tempo (quarter notes per minute)
SCORE_TRACK_API double LengthInSeconds(ScoreDuration d, int dots, double tempo);
//! Length of a bar in seconds
SCORE_TRACK_API double BarSeconds(const ScoreTimeSignature &ts, double tempo);
//! Frequency of a MIDI note
SCORE_TRACK_API double Frequency(int midi);

// ---- Rhythm: positions inside a bar are counted in 32nd notes
constexpr int UnitsPerWhole = 32;
//! Length of a note in 32nd notes
SCORE_TRACK_API int NoteUnits(ScoreDuration d, int dots);
//! Length of a bar in 32nd notes (6/8 is 24, 4/4 is 32...)
SCORE_TRACK_API int MeasureUnits(const ScoreTimeSignature &ts);

//! One rest of a gap
struct RestSlot {
   int start;                 //!< in 32nd notes from the start of the bar
   int units;
   ScoreDuration duration;
   int dots;
   bool wholeMeasure;         //!< the symbol that rests a complete bar
};
//! The rests that fill the gap [from, to) of a bar, split at the beats as
//! MuseScore does. A gap that is the complete bar gives one whole-bar rest.
SCORE_TRACK_API std::vector<RestSlot> FillGap(
   int from, int to, const ScoreTimeSignature &ts);
}

class ScoreTrack;

struct ScoreTrackEvent {
   enum Type { Selection };
   Type type;
   std::weak_ptr<ScoreTrack> pTrack;
};

//! A track that holds ONE staff of musical notation.
//! Multi-staff scores are several ScoreTracks sharing a group id.
class SCORE_TRACK_API ScoreTrack final
   : public UniqueChannelTrack<PlayableTrack>
   , public Observer::Publisher<ScoreTrackEvent>
{
public:
   //! Version of the XML written by WriteXML. Bump on any incompatible change.
   static constexpr long XML_VERSION = 1;
   static constexpr int DEFAULT_STAFF_LINES = 5;

   static wxString GetDefaultName();

   //! Construct and also build all attachments (used by the project file reader)
   static ScoreTrack *New(AudacityProject &project);

   //! Create a new ScoreTrack with a unique default name and append it to @p trackList
   static ScoreTrack *Create(TrackList &trackList);

   ScoreTrack();
   ScoreTrack(const ScoreTrack &orig, ProtectedCreationArg&&);
   ~ScoreTrack() override;

   int GetStaffLines() const { return mStaffLines; }
   void SetStaffLines(int lines);

   int64_t GetGroupId() const { return mGroupId; }
   void SetGroupId(int64_t id) { mGroupId = id; }

   //! Linear gain applied when the score is played (may change while playing)
   float GetGain() const { return mGain.load(); }
   void SetGain(float gain) { mGain.store(gain); }

   ScoreClef GetClef() const { return mClef; }
   void SetClef(ScoreClef clef) { mClef = clef; }
   ScoreKey GetKey() const { return mKey; }
   void SetKey(ScoreKey key) { mKey = key; }
   ScoreTimeSignature GetTimeSignature() const { return mTimeSignature; }
   void SetTimeSignature(ScoreTimeSignature ts) { mTimeSignature = ts; }

   // Notes (kept sorted by time)
   const std::vector<ScoreNote> &GetNotes() const { return mNotes; }
   //! Returns the index of the new note
   size_t AddNote(const ScoreNote &note);
   void RemoveNote(size_t index);

   // Bar lines are drawn by the user; nothing is generated from the tempo.
   const std::vector<double> &GetBarLines() const { return mBarLines; }

   //! A bar: the time between two consecutive bar lines (the first bar starts
   //! at time zero)
   struct Measure {
      double start;
      double end;
   };
   std::vector<Measure> GetMeasures() const;
   //! Removes the bar line closest to @p time, wherever it is
   bool RemoveNearestBarLine(double time);
   void AddBarLine(double time);
   //! Removes the bar line closest to @p time if it is within @p tolerance
   bool RemoveBarLineNear(double time, double tolerance);

   void SetSelected(bool s) override;

   const TypeInfo &GetTypeInfo() const override;
   static const TypeInfo &ClassTypeInfo();

   Track::Holder PasteInto(AudacityProject &project, TrackList &list)
      const override;

   // Persistence
   bool HandleXMLTag(const std::string_view& tag, const AttributesList& attrs) override;
   XMLTagHandler *HandleXMLChild(const std::string_view& tag) override;
   void WriteXML(XMLWriter &xmlFile) const override;

   // Editing
   Track::Holder Cut(double t0, double t1) override;
   Track::Holder Copy(double t0, double t1, bool forClipboard = true)
      const override;
   void Clear(double t0, double t1) override;
   void Paste(double t, const Track &src) override;
   void SyncLockAdjust(double oldT1, double newT1) override;
   void Silence(double t0, double t1, ProgressReporter reportProgress = {}) override;
   void InsertSilence(double t, double len) override;

   void MoveTo(double dOffset) override;
   void ShiftBy(double t0, double delta) override;

   size_t NIntervals() const override;

   //! The span of all the notes
   struct Interval final : WideChannelGroupInterval {
      explicit Interval(const ScoreTrack &track)
         : mpTrack{ track.SharedPointer<const ScoreTrack>() }
      {}
      ~Interval() override;
      double Start() const override;
      double End() const override;
      size_t NChannels() const override;
      std::shared_ptr<ChannelInterval> DoGetChannel(size_t iChannel) override;
   private:
      //! @invariant not null
      const std::shared_ptr<const ScoreTrack> mpTrack;
   };

   //! Time at which the last note stops sounding
   double NotesEnd() const;

private:
   Track::Holder Clone(bool backup) const override;
   std::shared_ptr<WideChannelGroupInterval> DoGetInterval(size_t iInterval)
      override;

   void SortNotes();
   //! Copies the staff settings (not the notes)
   void CopySettingsFrom(const ScoreTrack &other);

   std::atomic<float> mGain{ 1.0f };
   int mStaffLines{ DEFAULT_STAFF_LINES };
   int64_t mGroupId{ -1 };
   ScoreClef mClef{ ScoreClef::Treble };
   ScoreKey mKey{ ScoreKey::C };
   ScoreTimeSignature mTimeSignature{ 4, 4 };
   std::vector<ScoreNote> mNotes;
   std::vector<double> mBarLines;
};

ENUMERATE_TRACK_TYPE(ScoreTrack);
