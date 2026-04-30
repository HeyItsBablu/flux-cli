cmake_minimum_required(VERSION 3.22.1)
project(flux_cli LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG        master
)

FetchContent_MakeAvailable(miniz)

add_executable(flux
        src/main.cpp
        src/commands/create.cpp
        src/commands/run.cpp
        src/utils/fs.cpp
        src/utils/process.cpp
        src/utils/config.cpp
)

target_include_directories(flux PRIVATE
        src/
        ${miniz_SOURCE_DIR}
)

target_link_libraries(flux PRIVATE
        miniz
)

if(WIN32)
    target_link_libraries(flux PRIVATE ws2_32)
endif()