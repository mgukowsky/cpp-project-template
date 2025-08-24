set(FETCHCONTENT_QUIET FALSE)

if(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_FILE $ENV{CPM_SOURCE_CACHE}/CPM.cmake)
else()
  set(CPM_DOWNLOAD_FILE ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
endif()

# In the event that we're offline, CMake's idiotic behavior is to 
# replace the destination with an empty file, even if the file is
# non-empty!
if(NOT EXISTS "${CPM_DOWNLOAD_FILE}")
  # Per https://github.com/cpm-cmake/CPM.cmake/wiki/Downloading-CPM.cmake-in-CMake
  file(
    DOWNLOAD
    # https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
    https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
    ${CPM_DOWNLOAD_FILE}
    # EXPECTED_HASH
    #   SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
  )
endif()
include(${CPM_DOWNLOAD_FILE})
