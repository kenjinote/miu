cmake_minimum_required(VERSION 3.22.1)
project("miu")

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

# 起動コード(.c)を分離せず、main.cppと一緒に「miu」の中に直接組み込んでコンパイルする
add_library(miu SHARED
        main.cpp
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# グルーコードのヘッダファイルがある場所を指定
target_include_directories(miu PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
)

# 16KBアライメント強制（Pixel 8以降などの最新端末用）
target_link_options(miu PRIVATE "-Wl,-z,max-page-size=16384")

# 必要なOSライブラリをリンク
target_link_libraries(miu
        ${android-lib}
        ${log-lib}
        ${vulkan-lib}
)