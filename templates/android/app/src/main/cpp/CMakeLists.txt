cmake_minimum_required(VERSION 3.22.1)
project(flux_android LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
        flux
        GIT_REPOSITORY https://github.com/HeyItsBablu/flux.git
        GIT_TAG        main
)

FetchContent_MakeAvailable(flux)

# 5 levels up from android/app/src/main/cpp/ to repo root
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../)

add_library(native-lib SHARED
        ${PROJECT_ROOT}/main.cpp
)

target_link_libraries(native-lib PRIVATE
        flux::flux
        aaudio
        mediandk
        camera2ndk
        GLESv3
)

target_link_options(native-lib PRIVATE
        "-Wl,-u,ANativeActivity_onCreate"
        "-Wl,-u,createApp"
)