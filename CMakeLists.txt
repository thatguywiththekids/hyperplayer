cmake_minimum_required(VERSION 3.25)
project(hyperplayer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find dependencies
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENMPT REQUIRED libopenmpt>=0.6.0)

set(COMMON_SOURCES
    action_buttons.c
    app.c
    directory_listing.c
    mousecursor.c
    pattern_view.c
    player.c
    quadrascope.c
    renderer.c
    sample_display.c
    sample_list_usage_trigger.c
    sample_list.c
    spectrumanalyzer.c
    tunnelvisualizer.c
    ui.c
    urls.c
    vumeter.c
)

add_executable(hyperplayer main_sdl3.c ${COMMON_SOURCES})

target_link_libraries(hyperplayer PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    ${OPENMPT_LIBRARIES}
    m
)

target_include_directories(hyperplayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OPENMPT_INCLUDE_DIRS})
target_compile_definitions(hyperplayer PRIVATE _CRT_SECURE_NO_WARNINGS)

if(NOT WIN32)
    # Enable some POSIX features
    target_compile_definitions(hyperplayer PRIVATE _DEFAULT_SOURCE _GNU_SOURCE)
endif()

# Copy assets to build directory
file(COPY background.png protracker.ttf PT-mousecursor.png hyperplayer.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
