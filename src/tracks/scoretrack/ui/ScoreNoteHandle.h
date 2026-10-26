/*
* Audacity: A Digital Audio Editor
*/
#ifndef __AUDACITY_SCORE_NOTE_HANDLE__
#define __AUDACITY_SCORE_NOTE_HANDLE__

#include <wx/gdicmn.h>

#include "../../../UIHandle.h"

class ScoreTrack;

//! Adds a note where the staff is clicked (with the value chosen in the
//! floating palette), or removes the note that was clicked.
//! Only active on a selected score track; Ctrl, Shift or Alt give the usual
//! selection behaviour back.
class ScoreNoteHandle final : public UIHandle
{
public:
   ScoreNoteHandle(const std::shared_ptr<ScoreTrack> &pTrack, const wxRect &rect);
   ~ScoreNoteHandle() override;

   static UIHandlePtr HitTest(
      std::weak_ptr<ScoreNoteHandle> &holder,
      const wxMouseState &state, const wxRect &rect,
      const std::shared_ptr<ScoreTrack> &pTrack);

   Result Click(
      const TrackPanelMouseEvent &event, AudacityProject *pProject) override;
   Result Drag(
      const TrackPanelMouseEvent &event, AudacityProject *pProject) override;
   HitTestPreview Preview(
      const TrackPanelMouseState &state, AudacityProject *pProject) override;
   Result Release(
      const TrackPanelMouseEvent &event, AudacityProject *pProject,
      wxWindow *pParent) override;
   Result Cancel(AudacityProject *pProject) override;

   std::shared_ptr<const Track> FindTrack() const override;

private:
   std::shared_ptr<ScoreTrack> mpTrack;
   wxRect mRect;
};

#endif
