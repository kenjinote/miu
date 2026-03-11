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
# FreeType側のHarfBuzz依存は一旦切る（CMakeの循環参照ビルドエラーを防ぐため）
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz for now" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "Disable Brotli" FORCE)

FetchContent_MakeAvailable(freetype)
# --------------------------------------------------------------------


# --- 3. HarfBuzzの自動ダウンロードと組み込み (追加) ---
FetchContent_Declare(
        harfbuzz
        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
        GIT_TAG        8.3.0 # 安定バージョン
)

# HarfBuzz側のFreeType連携を有効化する (これが hb-ft.h を使うために必須)
set(HB_HAVE_FREETYPE ON CACHE BOOL "Enable FreeType in HarfBuzz" FORCE)
# エディタの描画用途では不要な機能を無効化してビルドを高速化・軽量化
set(HB_BUILD_SUBSET OFF CACHE BOOL "Disable HarfBuzz subsetting" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "Disable HarfBuzz utils" FORCE)
set(HB_HAVE_GLIB OFF CACHE BOOL "Disable GLib" FORCE)
set(HB_HAVE_ICU OFF CACHE BOOL "Disable ICU" FORCE)
set(HB_HAVE_INTROSPECTION OFF CACHE BOOL "Disable introspection" FORCE)

# 補足: HarfBuzzのCMakeが、FetchContentで取得したFreeTypeを認識できるようにする変数設定
set(FREETYPE_FOUND TRUE)
set(FREETYPE_INCLUDE_DIRS ${freetype_SOURCE_DIR}/include)
set(FREETYPE_LIBRARIES freetype)

FetchContent_MakeAvailable(harfbuzz)
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
        ${harfbuzz_SOURCE_DIR}/src  # ★ 追加: HarfBuzzの hb.h などがあるディレクトリ
)

target_link_options(miu PRIVATE "-Wl,-z,max-page-size=16384")

# miu に harfbuzz, freetype, png, zlib をリンクさせる
target_link_libraries(miu
        harfbuzz    # ★ 追加: 順番としてFreeTypeより前に書くか隣に書く
        freetype
        png_static
        ${z-lib}
        ${android-lib}
        ${log-lib}
        ${vulkan-lib}
)