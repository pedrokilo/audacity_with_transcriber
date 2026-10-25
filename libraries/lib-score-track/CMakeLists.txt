#[[
Library for the score (musical notation) track.
]]

set( SOURCES
   ScoreTrack.cpp
   ScoreTrack.h
)
set( LIBRARIES
   lib-track-interface
   lib-project-interface
)
audacity_library( lib-score-track "${SOURCES}" "${LIBRARIES}"
   "" ""
)
