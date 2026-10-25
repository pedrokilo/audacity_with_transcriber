/*
* Audacity: A Digital Audio Editor
*/
#include "ScoreTrack.h"

#include <algorithm>

#include "Project.h"
#include "XMLWriter.h"

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
   , mStaffLines{ orig.mStaffLines }
   , mGroupId{ orig.mGroupId }
{
}

ScoreTrack::~ScoreTrack() = default;

void ScoreTrack::SetStaffLines(int lines)
{
   mStaffLines = std::clamp(lines, 1, 12);
}

static const Track::TypeInfo &typeInfo()
{
   static Track::TypeInfo info{
      { "score", "score", XO("Score Track") }, true, &Track::ClassTypeInfo() };
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
   pNewTrack->mStaffLines = mStaffLines;
   pNewTrack->mGroupId = mGroupId;
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
   if (tag != "scoretrack")
      return false;

   for (auto pair : attrs) {
      const auto attr = pair.first;
      const auto value = pair.second;

      long n = 0;
      if (this->Track::HandleCommonXMLAttribute(attr, value)) {
      }
      else if (attr == "stafflines" && value.TryGet(n))
         SetStaffLines(static_cast<int>(n));
      else if (attr == "groupid" && value.TryGet(n))
         mGroupId = n;
      // "scoreversion" is read for forward compatibility; version 1 needs no migration.
   }
   return true;
}

XMLTagHandler *ScoreTrack::HandleXMLChild(const std::string_view &)
{
   return nullptr;
}

void ScoreTrack::WriteXML(XMLWriter &xmlFile) const
// may throw
{
   xmlFile.StartTag(wxT("scoretrack"));
   this->Track::WriteCommonXMLAttributes(xmlFile);
   xmlFile.WriteAttr(wxT("scoreversion"), XML_VERSION);
   xmlFile.WriteAttr(wxT("stafflines"), static_cast<long>(mStaffLines));
   xmlFile.WriteAttr(wxT("groupid"), static_cast<long>(mGroupId));
   xmlFile.EndTag(wxT("scoretrack"));
}

Track::Holder ScoreTrack::Cut(double t0, double t1)
{
   auto tmp = Copy(t0, t1);
   Clear(t0, t1);
   return tmp;
}

Track::Holder ScoreTrack::Copy(double, double, bool) const
{
   auto result = std::make_shared<ScoreTrack>();
   result->Init(*this);
   result->mStaffLines = mStaffLines;
   result->mGroupId = mGroupId;
   return result;
}

void ScoreTrack::Clear(double, double) {}
void ScoreTrack::Paste(double, const Track &) {}
void ScoreTrack::SyncLockAdjust(double, double) {}
void ScoreTrack::Silence(double, double, ProgressReporter) {}
void ScoreTrack::InsertSilence(double, double) {}
void ScoreTrack::MoveTo(double) {}
void ScoreTrack::ShiftBy(double, double) {}

size_t ScoreTrack::NIntervals() const
{
   return 0;
}

std::shared_ptr<WideChannelGroupInterval> ScoreTrack::DoGetInterval(size_t)
{
   return {};
}
