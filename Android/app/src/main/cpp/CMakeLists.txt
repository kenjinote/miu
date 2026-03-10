cmake_minimum_required(VERSION 3.22.1)
project("miu")

include(FetchContent)

# --- 1. libpngの自動ダウンロードと組み込み (カラー絵文字展開に必須) ---
FetchContent_Declare(
        libpng
        GIT_REPOSITORY https://github.com/glennrp/libpng.git
        GIT_TAG        v1.6.43 # 安定バージョン
)
# libpngの不要なビルド(共有ライブラリやテスト)を無効化して高速化
set(PNG_SHARED OFF CACHE BOOL "Disable shared png" FORCE)
set(PNG_TESTS OFF CACHE BOOL "Disable png tests" FORCE)
set(PNG_EXECUTABLES OFF CACHE BOOL "Disable png executables" FORCE)
FetchContent_MakeAvailable(libpng)

# ★ 修正ポイント: pnglibconf.h はビルドディレクトリに生成されるため、パスを2つ指定する
set(PNG_PNG_INCLUDE_DIR "${libpng_SOURCE_DIR};${libpng_BINARY_DIR}" CACHE PATH "libpng include dir" FORCE)
set(PNG_LIBRARY png_static CACHE STRING "libpng library" FORCE)
# --------------------------------------------------------------------

# --- 2. FreeTypeの自動ダウンロードと組み込み ---
FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG        VER-2-13-2 # 安定バージョン
)

# ★ PNG と ZLIB を有効化 (ZLIBはNDK標準のものを使用)
set(FT_DISABLE_ZLIB FALSE CACHE BOOL "Enable ZLIB" FORCE)
set(FT_DISABLE_PNG FALSE CACHE BOOL "Enable PNG" FORCE)

# その他は無効化のままでOK
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "Disable BZIP2" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz for now" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "Disable Brotli" FORCE)

FetchContent_MakeAvailable(freetype)
# --------------------------------------------------------------------

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)
# NDK標準の zlib を探しておく
find_library(z-lib z)

add_library(miu SHARED
        main.cpp
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(miu PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
        ${freetype_SOURCE_DIR}/include
)

target_link_options(miu PRIVATE "-Wl,-z,max-page-size=16384")

# miu に freetype, png, zlib をリンクさせる
target_link_libraries(miu
        freetype
        png_static  # ← png から png_static に変更
        ${z-lib}
        ${android-lib}
        ${log-lib}
        ${vulkan-lib}
)