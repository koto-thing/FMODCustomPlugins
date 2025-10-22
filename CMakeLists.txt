cmake_minimum_required(VERSION 3.10)

project(BitCrasher)

set(CMAKE_CXX_STANDARD 17)

# ---FMOD SDKのパス設定---
# サードパーティライブラリのパス設定
set(FMOD_THIRD_PARTY "${CMAKE_SOURCE_DIR}/ThirdParty")

# フォールバックのFMOD SDKパス設定
set(FMOD_SDK_FALLBACK "C:/Program Files/FMOD SoundSystem/FMOD Studio API Windows")

# インクルードディレクトリを設定
if(EXISTS "${FMOD_THIRD_PARTY}/inc")
    set(FMOD_INCLUDE_DIR "${FMOD_THIRD_PARTY}/inc")
else()
    set(FMOD_INCLUDE_DIR "${FMOD_SDK_FALLBACK}/api/core/inc")
endif()

# ---FAUST SDKのパス設定---
# FAUSTのインクルードディレクトリ設定
set(FAUST_SDK_FALLBACK "C:/Program Files/Faust")
set(FAUST_INCLUDE_DIRS "${FAUST_SDK_FALLBACK}/include")

# ライブラリファイルのパス設定
set(FMOD_LIB_PATH "")
if(EXISTS "${FMOD_THIRD_PARTY}/lib/fmod_vc.lib")
    set(FMOD_LIB_PATH "${FMOD_THIRD_PARTY}/lib/fmod_vc.lib")
elseif(EXISTS "${FMOD_THIRD_PARTY}/lib/fmodL_vc.lib")
    set(FMOD_LIB_PATH "${FMOD_THIRD_PARTY}/lib/fmodL_vc.lib")
elseif(EXISTS "${FMOD_SDK_FALLBACK}/api/core/lib/fmod_vc.lib")
    set(FMOD_LIB_PATH "${FMOD_SDK_FALLBACK}/api/core/lib/fmod_vc.lib")
endif()

set(FAUST_LIB_PATH "")
if(EXISTS "${FAUST_SDK_FALLBACK}/lib/faust.lib")
    set(FAUST_LIB_PATH "${FAUST_SDK_FALLBACK}/lib/faust.lib")
endif()

message(STATUS "FMOD include dir: ${FMOD_INCLUDE_DIR}")
if(FMOD_LIB_PATH)
    message(STATUS "FMOD lib: ${FMOD_LIB_PATH}")
else()
    message(WARNING "FMOD library not found in ThirdParty or fallback path. You may need to adjust paths.")
endif()

message(STATUS "FAUST include dir: ${FAUST_INCLUDE_DIRS}")
if(FAUST_LIB_PATH)
    message(STATUS "FAUST lib: ${FAUST_LIB_PATH}")
else()
    message(WARNING "FAUST library not found in fallback path. You may need to adjust paths.")
endif()


# ソースファイルの設定
set(SOURCE_FILES
        BitCrasher/BitCrasher.cpp
        BitCrasher/FaustBitCrasher.h
)

# ライブラリの作成
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# インクルードディレクトリの追加
target_include_directories(${PROJECT_NAME} PRIVATE ${FMOD_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${FAUST_INCLUDE_DIRS})

if(FMOD_LIB_PATH)
    # ライブラリのリンク
    target_link_libraries(${PROJECT_NAME} PRIVATE "${FMOD_LIB_PATH}")
endif()

if(FAUST_LIB_PATH)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${FAUST_LIB_PATH}")
endif()
