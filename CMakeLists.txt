cmake_minimum_required(VERSION 3.21)

include("$ENV{PSN00BSDK_LIBS}/cmake/sdk.cmake")
project(vizo_mario_psx C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/system.cnf"
"BOOT=cdrom:\\VIZO.EXE;1\nTCB=4\nEVENT=10\nSTACK=801FFFF0\n")

configure_file("${PROJECT_SOURCE_DIR}/iso.xml.in"
               "${CMAKE_CURRENT_BINARY_DIR}/iso.xml" @ONLY)

psn00bsdk_add_executable(vizo GPREL
    src/main.c
)

psn00bsdk_add_cd_image(vizo_disc VIZO_MARIO
    "${CMAKE_CURRENT_BINARY_DIR}/iso.xml"
    DEPENDS vizo "${CMAKE_CURRENT_BINARY_DIR}/system.cnf"
)
