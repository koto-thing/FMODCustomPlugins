cmake_minimum_required(VERSION 3.10)

project(BitCrasher)

set(CMAKE_CXX_STANDARD 17)

# Use local ThirdParty folder (relative to the project root) instead of system Program Files.
set(FMOD_THIRD_PARTY "${CMAKE_SOURCE_DIR}/ThirdParty")

# Prefer the local include/lib; if not found, fall back to an external SDK path (kept for compatibility).
set(FMOD_SDK_FALLBACK "C:/Program Files/FMOD SoundSystem/FMOD Studio API Windows")

# Choose include dir: prefer ThirdParty/inc
if(EXISTS "${FMOD_THIRD_PARTY}/inc")
    set(FMOD_INCLUDE_DIR "${FMOD_THIRD_PARTY}/inc")
else()
    set(FMOD_INCLUDE_DIR "${FMOD_SDK_FALLBACK}/api/core/inc")
endif()

# Choose library file: prefer ThirdParty/lib/fmod_vc.lib (or fmodL_vc.lib)
set(FMOD_LIB_PATH "")
if(EXISTS "${FMOD_THIRD_PARTY}/lib/fmod_vc.lib")
    set(FMOD_LIB_PATH "${FMOD_THIRD_PARTY}/lib/fmod_vc.lib")
elseif(EXISTS "${FMOD_THIRD_PARTY}/lib/fmodL_vc.lib")
    set(FMOD_LIB_PATH "${FMOD_THIRD_PARTY}/lib/fmodL_vc.lib")
elseif(EXISTS "${FMOD_SDK_FALLBACK}/api/core/lib/fmod_vc.lib")
    set(FMOD_LIB_PATH "${FMOD_SDK_FALLBACK}/api/core/lib/fmod_vc.lib")
endif()

message(STATUS "FMOD include dir: ${FMOD_INCLUDE_DIR}")
if(FMOD_LIB_PATH)
    message(STATUS "FMOD lib: ${FMOD_LIB_PATH}")
else()
    message(WARNING "FMOD library not found in ThirdParty or fallback path. You may need to adjust paths.")
endif()

# Source files
set(SOURCE_FILES BitCrasher/BitCrasher.cpp)

# Correct function name: add_library (lowercase L) and use the project name
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Prefer target-based include/linking
target_include_directories(${PROJECT_NAME} PRIVATE ${FMOD_INCLUDE_DIR})

if(FMOD_LIB_PATH)
    # Link the .lib directly by full path (Windows .lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${FMOD_LIB_PATH}")
endif()
