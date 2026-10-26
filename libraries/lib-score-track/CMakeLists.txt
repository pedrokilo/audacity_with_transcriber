#[[
Library for the score (musical notation) track.
]]

set( SOURCES
   ScoreSequence.cpp
   ScoreSequence.h
   ScoreTrack.cpp
   ScoreTrack.h
)
set( LIBRARIES
   lib-track-interface
   lib-mixer-interface
   lib-playable-track-interface
   lib-project-interface
)
audacity_library( lib-score-track "${SOURCES}" "${LIBRARIES}"
   "" ""
)
