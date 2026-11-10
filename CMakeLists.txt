# Pomodoro City for PSP - based on the official PSPDEV template
# https://pspdev.github.io/basic_programs.html
#
# Build:
#   mkdir build && cd build
#   psp-cmake ..
#   make
cmake_minimum_required(VERSION 3.11)

project(Pomodoro C)

add_executable(${PROJECT_NAME}
    src/main.c
    src/core.c
    src/gfx.c
    src/audio.c
    src/storage.c
    src/datetime.c
    src/city.c
    src/views.c
    src/assets.c
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_options(${PROJECT_NAME} PRIVATE -O2 -Wall)

# pspaudiolib is listed before pspaudio because it uses pspaudio's functions
target_link_libraries(${PROJECT_NAME} PRIVATE
    pspdebug
    pspdisplay
    pspge
    pspctrl
    pspaudiolib
    pspaudio
    psputility
    psppower
    psprtc
    m
)

# Create the EBOOT.PBP file, with the XMB icon and background
create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH ${CMAKE_SOURCE_DIR}/assets/ICON0.PNG
    BACKGROUND_PATH ${CMAKE_SOURCE_DIR}/assets/PIC1.PNG
    PREVIEW_PATH NULL
    TITLE "Pomodoro City"
    VERSION 02.10
)
