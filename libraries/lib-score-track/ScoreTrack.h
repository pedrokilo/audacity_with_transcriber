/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>

#include "Track.h"

class AudacityProject;

//! A track that holds ONE staff of musical notation.
//! Multi-staff scores are several ScoreTracks sharing a group id.
//! Prototype: no musical content yet; it only draws an empty staff and persists its settings.
class SCORE_TRACK_API ScoreTrack final : public UniqueChannelTrack<>
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

   const TypeInfo &GetTypeInfo() const override;
   static const TypeInfo &ClassTypeInfo();

   Track::Holder PasteInto(AudacityProject &project, TrackList &list)
      const override;

   // Persistence
   bool HandleXMLTag(const std::string_view& tag, const AttributesList& attrs) override;
   XMLTagHandler *HandleXMLChild(const std::string_view& tag) override;
   void WriteXML(XMLWriter &xmlFile) const override;

   // Editing: an empty score has no time content, so these are no-ops for now.
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

private:
   Track::Holder Clone(bool backup) const override;
   std::shared_ptr<WideChannelGroupInterval> DoGetInterval(size_t iInterval)
      override;

   int mStaffLines{ DEFAULT_STAFF_LINES };
   int64_t mGroupId{ -1 };
};

ENUMERATE_TRACK_TYPE(ScoreTrack);
