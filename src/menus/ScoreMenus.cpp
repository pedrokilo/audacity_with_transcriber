/*
* Audacity: A Digital Audio Editor
*/
#include "AudioIO.h"
#include "BasicUI.h"
#include "../CommonCommandFlags.h"
#include "Project.h"
#include "ProjectHistory.h"
#include "ScoreTrack.h"
#include "../SelectUtilities.h"
#include "TrackFocus.h"
#include "../TrackPanel.h"
#include "Viewport.h"
#include "CommandContext.h"
#include "toolbars/ToolManager.h"

namespace {
void OnNewScoreTrack(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get(project);

   auto track = ScoreTrack::Create(tracks);

   SelectUtilities::SelectNone(project);

   track->SetSelected(true);

   ProjectHistory::Get(project)
      .PushState(XO("Created new score track"), XO("New Track"));

   TrackFocus::Get(project).Set(track);
   Viewport::Get(project).ShowTrack(*track);
}

using namespace MenuRegistry;

AttachedItem sAttachment{
   Command(wxT("NewScoreTrack"), XXO("&Score Track"),
      OnNewScoreTrack, AudioIONotBusyFlag()),
   wxT("Tracks/Add/Add")
};
}
