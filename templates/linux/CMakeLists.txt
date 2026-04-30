cmake_minimum_required(VERSION 3.22.1)
project(flux_app LANGUAGES CXX C)

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

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(app
        ${PROJECT_ROOT}/main.cpp
        ${flux_SOURCE_DIR}/src/main_linux.cpp
)

target_link_libraries(app PRIVATE
        flux::flux
)

target_include_directories(app PRIVATE
        ${flux_SOURCE_DIR}/src
)