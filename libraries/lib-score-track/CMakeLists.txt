#[[
Library for the score (musical notation) track.
]]

set( SOURCES
   ScoreSequence.cpp
   ScoreSequence.h
   ScoreTrack.cpp
   ScoreTrack.h
   ScoreTrackEditing.cpp
)
set( LIBRARIES
   lib-track-interface
   lib-mixer-interface
   lib-playable-track-interface
   lib-stretching-sequence-interface
   lib-project-interface
)
audacity_library( lib-score-track "${SOURCES}" "${LIBRARIES}"
   "" ""
)
