cmake_minimum_required(VERSION 3.22.1)
project("miu")

# --- FreeTypeの自動ダウンロードと組み込み ---
include(FetchContent)
FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG        VER-2-13-2 # 安定バージョン
)
# FreeTypeの不要なオプションを無効化してビルドを高速化
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "Disable ZLIB" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "Disable BZIP2" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "Disable PNG" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz for now" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "Disable Brotli" FORCE)
FetchContent_MakeAvailable(freetype)
# ----------------------------------------

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

add_library(miu SHARED
        main.cpp
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(miu PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
)

# FreeTypeのヘッダパスを通す
target_include_directories(miu PRIVATE ${freetype_SOURCE_DIR}/include)

target_link_options(miu PRIVATE "-Wl,-z,max-page-size=16384")

# freetype をリンクに追加
target_link_libraries(miu
        freetype
        ${android-lib}
        ${log-lib}
        ${vulkan-lib}
)