cmake_minimum_required(VERSION 3.10)

project(naraku_psp C)

option(NARAKU_COMPRESSED_ASSETS "Read DEFLATE-compressed texture/audio assets" OFF)
option(NARAKU_BOOT_DIAGNOSTICS "Show and log PSP startup checkpoints" OFF)
set(NARAKU_GAME_DIR "NARAKU" CACHE STRING "Folder name under ms0:/PSP/GAME/")

add_executable(${PROJECT_NAME}
    main.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -O2
    -Wall
    -Wextra
)

if(NARAKU_BOOT_DIAGNOSTICS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NARAKU_BOOT_DIAGNOSTICS=1)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE NARAKU_GAME_DIR=\"${NARAKU_GAME_DIR}\")

target_link_libraries(${PROJECT_NAME}
    pspdebug pspgu pspge pspdisplay pspctrl pspaudio pspkernel
)

if(NARAKU_COMPRESSED_ASSETS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NARAKU_COMPRESSED_ASSETS=1)
    target_link_libraries(${PROJECT_NAME} z)
endif()

set(NARAKU_XMB_ARGS)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xmb/ICON0.PNG")
    list(APPEND NARAKU_XMB_ARGS ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/xmb/ICON0.PNG")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xmb/PIC1.PNG")
    list(APPEND NARAKU_XMB_ARGS BACKGROUND_PATH "${CMAKE_CURRENT_SOURCE_DIR}/xmb/PIC1.PNG")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xmb/SND0.AT3")
    list(APPEND NARAKU_XMB_ARGS MUSIC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/xmb/SND0.AT3")
endif()

create_pbp_file(
    TARGET ${PROJECT_NAME}
    TITLE "NARAKU PSP"
    VERSION 01.78
    ${NARAKU_XMB_ARGS}
)
