/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrack.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Project.h"
#include "XMLWriter.h"

// ---------------------------------------------------------------------------
// ScoreTheory
// ---------------------------------------------------------------------------
namespace ScoreTheory {
namespace {
// Letters in the order sharps and flats are added to a key signature
constexpr int SharpOrder[7] = { 3, 0, 4, 1, 5, 2, 6 }; // F C G D A E B
constexpr int FlatOrder[7] = { 6, 2, 5, 1, 4, 0, 3 };  // B E A D G C F
}

int BottomLineDia(ScoreClef clef)
{
   // Treble: bottom line is E4. Bass: bottom line is G2.
   return clef == ScoreClef::Bass ? 2 * 7 + 4 : 4 * 7 + 2;
}

int Midi(int dia, int alter)
{
   static constexpr int semitones[7] = { 0, 2, 4, 5, 7, 9, 11 };
   const int octave = dia / 7;
   const int letter = dia % 7;
   return 12 * (octave + 1) + semitones[letter] + alter;
}

int KeyAccidentalCount(ScoreKey key)
{
   switch (key) {
   case ScoreKey::C:  return 0;
   case ScoreKey::Cm: return -3;
   case ScoreKey::A:  return 3;
   case ScoreKey::Am: return 0;
   case ScoreKey::G:  return 1;
   case ScoreKey::Gm: return -2;
   case ScoreKey::F:  return -1;
   case ScoreKey::Fm: return -4;
   default:           return 0;
   }
}

int KeyAlteration(ScoreKey key, int letter)
{
   const int count = KeyAccidentalCount(key);
   if (count > 0) {
      for (int i = 0; i < count; ++i)
         if (SharpOrder[i] == letter)
            return 1;
   }
   else if (count < 0) {
      for (int i = 0; i < -count; ++i)
         if (FlatOrder[i] == letter)
            return -1;
   }
   return 0;
}

double LengthInWholeNotes(ScoreDuration d, int dots)
{
   double length = 1.0 / static_cast<double>(1 << static_cast<int>(d));
   double extra = length;
   double total = length;
   for (int i = 0; i < dots; ++i) {
      extra /= 2.0;
      total += extra;
   }
   return total;
}

double LengthInSeconds(ScoreDuration d, int dots, double tempo)
{
   if (tempo <= 0.0)
      tempo = 120.0;
   // A quarter note lasts 60 / tempo seconds and is a quarter of a whole note
   return LengthInWholeNotes(d, dots) * 4.0 * 60.0 / tempo;
}

double BarSeconds(const ScoreTimeSignature &ts, double tempo)
{
   if (tempo <= 0.0)
      tempo = 120.0;
   const int den = std::max(1, ts.denominator);
   return (static_cast<double>(ts.numerator) / den) * 4.0 * 60.0 / tempo;
}

double Frequency(int midi)
{
   return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

int NoteUnits(ScoreDuration d, int dots)
{
   return std::max(1, static_cast<int>(
      std::lround(LengthInWholeNotes(d, dots) * UnitsPerWhole)));
}

int MeasureUnits(const ScoreTimeSignature &ts)
{
   return std::max(1, ts.numerator * UnitsPerWhole / std::max(1, ts.denominator));
}

namespace {
struct RestValue {
   int units;
   int align;                 //!< the position must be a multiple of this
   ScoreDuration duration;
   int dots;
};
// From the longest to the shortest
const RestValue RestValues[] = {
   { 32, 32, ScoreDuration::Whole, 0 },
   { 24, 16, ScoreDuration::Half, 1 },
   { 16, 16, ScoreDuration::Half, 0 },
   { 12, 8, ScoreDuration::Quarter, 1 },
   { 8, 8, ScoreDuration::Quarter, 0 },
   { 6, 4, ScoreDuration::Eighth, 1 },
   { 4, 4, ScoreDuration::Eighth, 0 },
   { 3, 2, ScoreDuration::Sixteenth, 1 },
   { 2, 2, ScoreDuration::Sixteenth, 0 },
   { 1, 1, ScoreDuration::ThirtySecond, 0 },
};
}

std::vector<RestSlot> FillGap(int from, int to, const ScoreTimeSignature &ts)
{
   std::vector<RestSlot> slots;
   const int bar = MeasureUnits(ts);
   if (to <= from)
      return slots;

   // A gap that is the whole bar: the symbol that rests a complete bar
   if (from <= 0 && to >= bar) {
      slots.push_back({ 0, bar, ScoreDuration::Whole, 0, true });
      return slots;
   }

   // Compound time (6/8, 9/8, 12/8) beats on the dotted quarter
   const bool compound = ts.denominator == 8 && ts.numerator >= 6
      && ts.numerator % 3 == 0;
   const int beat = compound ? 12 : std::max(1, UnitsPerWhole / ts.denominator);

   int a = from;
   while (a < to) {
      const int remaining = to - a;
      bool placed = false;
      for (const auto &value : RestValues) {
         if (value.units > remaining || a % value.align != 0)
            continue;
         // A rest longer than a beat must be made of whole beats
         if (value.units > beat && value.units % beat != 0)
            continue;
         // Off the beat, do not cross into the next one
         if (a % beat != 0 && a / beat != (a + value.units - 1) / beat)
            continue;
         slots.push_back({ a, value.units, value.duration, value.dots, false });
         a += value.units;
         placed = true;
         break;
      }
      if (!placed) {
         slots.push_back({ a, 1, ScoreDuration::ThirtySecond, 0, false });
         ++a;
      }
   }
   return slots;
}
}

// ---------------------------------------------------------------------------
// ScoreTrack
// ---------------------------------------------------------------------------
static ProjectFileIORegistry::ObjectReaderEntry readerEntry{
   "scoretrack",
   ScoreTrack::New
};

wxString ScoreTrack::GetDefaultName()
{
   return _("Score");
}

ScoreTrack *ScoreTrack::New(AudacityProject &project)
{
   auto &tracks = TrackList::Get(project);
   auto result = tracks.Add(std::make_shared<ScoreTrack>());
   result->AttachedTrackObjects::BuildAll();
   return result;
}

ScoreTrack *ScoreTrack::Create(TrackList &trackList)
{
   auto track = std::make_shared<ScoreTrack>();
   track->SetName(trackList.MakeUniqueTrackName(GetDefaultName()));
   trackList.Add(track);
   return track.get();
}

ScoreTrack::ScoreTrack()
   : UniqueChannelTrack{}
{
}

ScoreTrack::ScoreTrack(const ScoreTrack &orig, ProtectedCreationArg &&a)
   : UniqueChannelTrack{ orig, std::move(a) }
   , mNotes{ orig.mNotes }
   , mBarLines{ orig.mBarLines }
{
   mGain.store(orig.GetGain());
   CopySettingsFrom(orig);
}

ScoreTrack::~ScoreTrack() = default;

void ScoreTrack::CopySettingsFrom(const ScoreTrack &other)
{
   mGain.store(other.GetGain());
   mStaffLines = other.mStaffLines;
   mGroupId = other.mGroupId;
   mClef = other.mClef;
   mKey = other.mKey;
   mTimeSignature = other.mTimeSignature;
}

void ScoreTrack::SetStaffLines(int lines)
{
   mStaffLines = std::clamp(lines, 1, 12);
}

void ScoreTrack::SortNotes()
{
   std::stable_sort(mNotes.begin(), mNotes.end(),
      [](const ScoreNote &a, const ScoreNote &b) { return a.time < b.time; });
}

size_t ScoreTrack::AddNote(const ScoreNote &note)
{
   auto it = std::upper_bound(mNotes.begin(), mNotes.end(), note,
      [](const ScoreNote &a, const ScoreNote &b) { return a.time < b.time; });
   const auto index = static_cast<size_t>(it - mNotes.begin());
   mNotes.insert(it, note);
   return index;
}

void ScoreTrack::RemoveNote(size_t index)
{
   if (index < mNotes.size())
      mNotes.erase(mNotes.begin() + index);
}

void ScoreTrack::AddBarLine(double time)
{
   // Ignore a bar line drawn on top of another
   for (double t : mBarLines)
      if (std::abs(t - time) < 1e-4)
         return;
   mBarLines.insert(
      std::upper_bound(mBarLines.begin(), mBarLines.end(), time), time);
}

std::vector<ScoreTrack::Measure> ScoreTrack::GetMeasures() const
{
   std::vector<double> bounds{ 0.0 };
   for (double t : mBarLines)
      if (t > bounds.back() + 1e-3)
         bounds.push_back(t);
   std::vector<Measure> measures;
   for (size_t i = 0; i + 1 < bounds.size(); ++i)
      measures.push_back({ bounds[i], bounds[i + 1] });
   return measures;
}

void ScoreTrack::ScaleTime(double ratio)
{
   for (auto &note : mNotes) {
      note.time *= ratio;
      note.length *= ratio;
   }
   for (auto &bar : mBarLines)
      bar *= ratio;
}

bool ScoreTrack::RemoveNearestBarLine(double time)
{
   return RemoveBarLineNear(time, std::numeric_limits<double>::max());
}

bool ScoreTrack::RemoveBarLineNear(double time, double tolerance)
{
   auto best = mBarLines.end();
   double bestDistance = tolerance;
   for (auto it = mBarLines.begin(); it != mBarLines.end(); ++it) {
      const double distance = std::abs(*it - time);
      if (distance <= bestDistance) {
         bestDistance = distance;
         best = it;
      }
   }
   if (best == mBarLines.end())
      return false;
   mBarLines.erase(best);
   return true;
}

void ScoreTrack::SetSelected(bool s)
{
   const bool selected = GetSelected();
   Track::SetSelected(s);
   if (selected != GetSelected())
      Publish({ ScoreTrackEvent::Selection, SharedPointer<ScoreTrack>() });
}

static const Track::TypeInfo &typeInfo()
{
   static Track::TypeInfo info{
      { "score", "score", XO("Score Track") }, true, &PlayableTrack::ClassTypeInfo() };
   return info;
}

auto ScoreTrack::GetTypeInfo() const -> const TypeInfo &
{
   return typeInfo();
}

auto ScoreTrack::ClassTypeInfo() -> const TypeInfo &
{
   return typeInfo();
}

Track::Holder ScoreTrack::PasteInto(AudacityProject &, TrackList &list) const
{
   auto pNewTrack = std::make_shared<ScoreTrack>();
   pNewTrack->Init(*this);
   pNewTrack->CopySettingsFrom(*this);
   pNewTrack->mNotes = mNotes;
   pNewTrack->mBarLines = mBarLines;
   list.Add(pNewTrack);
   return pNewTrack;
}

Track::Holder ScoreTrack::Clone(bool) const
{
   auto result = std::make_shared<ScoreTrack>(*this, ProtectedCreationArg{});
   result->Init(*this);
   return result;
}

bool ScoreTrack::HandleXMLTag(const std::string_view &tag, const AttributesList &attrs)
{
   if (tag == "scoretrack") {
      for (auto pair : attrs) {
         const auto attr = pair.first;
         const auto value = pair.second;

         long n = 0;
         float gain = 1.0f;
         if (this->Track::HandleCommonXMLAttribute(attr, value)) {
         }
         else if (this->PlayableTrack::HandleXMLAttribute(attr, value)) {
         }
         else if (attr == "gain" && value.TryGet(gain))
            SetGain(gain);
         else if (attr == "stafflines" && value.TryGet(n))
            SetStaffLines(static_cast<int>(n));
         else if (attr == "groupid" && value.TryGet(n))
            mGroupId = n;
         else if (attr == "clef" && value.TryGet(n))
            mClef = n == 1 ? ScoreClef::Bass : ScoreClef::Treble;
         else if (attr == "key" && value.TryGet(n)
                  && n >= 0 && n < static_cast<long>(ScoreKey::Count))
            mKey = static_cast<ScoreKey>(n);
         else if (attr == "timenum" && value.TryGet(n) && n > 0 && n <= 32)
            mTimeSignature.numerator = static_cast<int>(n);
         else if (attr == "timeden" && value.TryGet(n) && n > 0 && n <= 32)
            mTimeSignature.denominator = static_cast<int>(n);
         // "scoreversion" is read for forward compatibility.
      }
      mNotes.clear();
      mBarLines.clear();
      return true;
   }

   if (tag == "bar") {
      for (auto pair : attrs) {
         double d = 0.0;
         if (pair.first == "t" && pair.second.TryGet(d))
            mBarLines.push_back(d);
      }
      return true;
   }

   if (tag == "note") {
      ScoreNote note;
      for (auto pair : attrs) {
         const auto attr = pair.first;
         const auto value = pair.second;

         long n = 0;
         double d = 0.0;
         if (attr == "t" && value.TryGet(d))
            note.time = d;
         else if (attr == "len" && value.TryGet(d) && d > 0.0)
            note.length = d;
         else if (attr == "dia" && value.TryGet(n))
            note.dia = static_cast<int>(n);
         else if (attr == "alter" && value.TryGet(n))
            note.alter = static_cast<int>(n);
         else if (attr == "acc" && value.TryGet(n)
                  && n >= 0 && n <= static_cast<long>(ScoreAccidental::Flat))
            note.accidental = static_cast<ScoreAccidental>(n);
         else if (attr == "dur" && value.TryGet(n)
                  && n >= 0 && n < static_cast<long>(ScoreDuration::Count))
            note.duration = static_cast<ScoreDuration>(n);
         else if (attr == "dots" && value.TryGet(n))
            note.dots = static_cast<int>(std::clamp(n, 0L, 3L));
         else if (attr == "rest" && value.TryGet(n))
            note.rest = n != 0;
         else if (attr == "tie" && value.TryGet(n))
            note.tie = n != 0;
      }
      mNotes.push_back(note);
      return true;
   }

   return false;
}

XMLTagHandler *ScoreTrack::HandleXMLChild(const std::string_view &tag)
{
   if (tag == "note" || tag == "bar")
      return this;
   return nullptr;
}

void ScoreTrack::WriteXML(XMLWriter &xmlFile) const
// may throw
{
   xmlFile.StartTag(wxT("scoretrack"));
   this->Track::WriteCommonXMLAttributes(xmlFile);
   this->PlayableTrack::WriteXMLAttributes(xmlFile);
   xmlFile.WriteAttr(wxT("gain"), static_cast<double>(GetGain()), 6);
   xmlFile.WriteAttr(wxT("scoreversion"), XML_VERSION);
   xmlFile.WriteAttr(wxT("stafflines"), static_cast<long>(mStaffLines));
   xmlFile.WriteAttr(wxT("groupid"), static_cast<long>(mGroupId));
   xmlFile.WriteAttr(wxT("clef"), static_cast<long>(mClef));
   xmlFile.WriteAttr(wxT("key"), static_cast<long>(mKey));
   xmlFile.WriteAttr(wxT("timenum"), static_cast<long>(mTimeSignature.numerator));
   xmlFile.WriteAttr(wxT("timeden"), static_cast<long>(mTimeSignature.denominator));

   for (const auto &note : mNotes) {
      xmlFile.StartTag(wxT("note"));
      xmlFile.WriteAttr(wxT("t"), note.time, 8);
      xmlFile.WriteAttr(wxT("len"), note.length, 8);
      xmlFile.WriteAttr(wxT("dia"), static_cast<long>(note.dia));
      xmlFile.WriteAttr(wxT("alter"), static_cast<long>(note.alter));
      xmlFile.WriteAttr(wxT("acc"), static_cast<long>(note.accidental));
      xmlFile.WriteAttr(wxT("dur"), static_cast<long>(note.duration));
      xmlFile.WriteAttr(wxT("dots"), static_cast<long>(note.dots));
      xmlFile.WriteAttr(wxT("rest"), static_cast<long>(note.rest ? 1 : 0));
      xmlFile.WriteAttr(wxT("tie"), static_cast<long>(note.tie ? 1 : 0));
      xmlFile.EndTag(wxT("note"));
   }

   for (double t : mBarLines) {
      xmlFile.StartTag(wxT("bar"));
      xmlFile.WriteAttr(wxT("t"), t, 8);
      xmlFile.EndTag(wxT("bar"));
   }

   xmlFile.EndTag(wxT("scoretrack"));
}

Track::Holder ScoreTrack::Cut(double t0, double t1)
{
   auto tmp = Copy(t0, t1);
   Clear(t0, t1);
   return tmp;
}

Track::Holder ScoreTrack::Copy(double t0, double t1, bool) const
{
   auto result = std::make_shared<ScoreTrack>();
   result->Init(*this);
   result->CopySettingsFrom(*this);
   for (const auto &note : mNotes) {
      if (note.time >= t0 && note.time < t1) {
         ScoreNote copy = note;
         copy.time -= t0;
         result->mNotes.push_back(copy);
      }
   }
   for (double bar : mBarLines)
      if (bar >= t0 && bar < t1)
         result->mBarLines.push_back(bar - t0);
   return result;
}

void ScoreTrack::Clear(double t0, double t1)
{
   const double length = t1 - t0;
   mNotes.erase(std::remove_if(mNotes.begin(), mNotes.end(),
      [&](const ScoreNote &n) { return n.time >= t0 && n.time < t1; }),
      mNotes.end());
   for (auto &note : mNotes)
      if (note.time >= t1)
         note.time -= length;

   mBarLines.erase(std::remove_if(mBarLines.begin(), mBarLines.end(),
      [&](double t) { return t >= t0 && t < t1; }), mBarLines.end());
   for (auto &bar : mBarLines)
      if (bar >= t1)
         bar -= length;
}

void ScoreTrack::Paste(double t, const Track &src)
{
   const auto *other = dynamic_cast<const ScoreTrack *>(&src);
   if (!other)
      return;
   double length = 0.0;
   for (const auto &note : other->mNotes)
      length = std::max(length, note.time + note.length);
   for (double bar : other->mBarLines)
      length = std::max(length, bar);
   // Make room, then insert
   for (auto &note : mNotes)
      if (note.time >= t)
         note.time += length;
   for (auto &bar : mBarLines)
      if (bar >= t)
         bar += length;
   for (double bar : other->mBarLines)
      mBarLines.push_back(bar + t);
   std::sort(mBarLines.begin(), mBarLines.end());
   for (const auto &note : other->mNotes) {
      ScoreNote copy = note;
      copy.time += t;
      mNotes.push_back(copy);
   }
   SortNotes();
}

void ScoreTrack::SyncLockAdjust(double, double) {}

void ScoreTrack::Silence(double t0, double t1, ProgressReporter)
{
   mNotes.erase(std::remove_if(mNotes.begin(), mNotes.end(),
      [&](const ScoreNote &n) { return n.time >= t0 && n.time < t1; }),
      mNotes.end());
}

void ScoreTrack::InsertSilence(double t, double len)
{
   for (auto &note : mNotes)
      if (note.time >= t)
         note.time += len;
   for (auto &bar : mBarLines)
      if (bar >= t)
         bar += len;
}

void ScoreTrack::MoveTo(double dOffset)
{
   if (mNotes.empty())
      return;
   ShiftBy(mNotes.front().time, dOffset - mNotes.front().time);
}

void ScoreTrack::ShiftBy(double t0, double delta)
{
   for (auto &note : mNotes)
      if (note.time >= t0)
         note.time += delta;
   for (auto &bar : mBarLines)
      if (bar >= t0)
         bar += delta;
}

size_t ScoreTrack::NIntervals() const
{
   return mNotes.empty() ? 0 : 1;
}

double ScoreTrack::NotesEnd() const
{
   double end = 0.0;
   for (const auto &note : mNotes)
      end = std::max(end, note.time + note.length);
   return end;
}

std::shared_ptr<WideChannelGroupInterval> ScoreTrack::DoGetInterval(size_t iInterval)
{
   if (iInterval == 0 && !mNotes.empty())
      return std::make_shared<Interval>(*this);
   return {};
}

ScoreTrack::Interval::~Interval() = default;

double ScoreTrack::Interval::Start() const
{
   const auto &notes = mpTrack->GetNotes();
   return notes.empty() ? 0.0 : notes.front().time;
}

double ScoreTrack::Interval::End() const
{
   return mpTrack->NotesEnd();
}

size_t ScoreTrack::Interval::NChannels() const
{
   return 1;
}

std::shared_ptr<ChannelInterval> ScoreTrack::Interval::DoGetChannel(size_t iChannel)
{
   if (iChannel == 0)
      return std::make_shared<ChannelInterval>();
   return {};
}
