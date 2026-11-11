# Інсталятор LumioPhoto-Setup.exe (один файл). Збирається скриптом build.bat
# ПІСЛЯ того, як програму зібрано і запаковано в payload.zip.
cmake_minimum_required(VERSION 3.16)
project(LumioPhotoSetup LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PAYLOAD_ZIP "" CACHE FILEPATH "zip-архів із готовою програмою (папка dist)")
if(NOT EXISTS "${PAYLOAD_ZIP}")
    message(FATAL_ERROR "Не знайдено PAYLOAD_ZIP: '${PAYLOAD_ZIP}'. Запустіть build.bat з кореня проєкту.")
endif()

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# версія — та сама, що в програмі (кореневий CMakeLists.txt)
file(READ "${ROOT_DIR}/CMakeLists.txt" ROOT_CMAKE)
string(REGEX MATCH "project\\([A-Za-z]+ VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${ROOT_CMAKE}")
set(APP_VERSION "${CMAKE_MATCH_1}")
if(NOT APP_VERSION)
    message(FATAL_ERROR "Не вдалося прочитати версію з ${ROOT_DIR}/CMakeLists.txt")
endif()
string(REPLACE "." "," APP_VERSION_COMMA "${APP_VERSION}")
message(STATUS "Версія: ${APP_VERSION}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version.h" "#pragma once\n#define APP_VERSION_W L\"${APP_VERSION}\"\n")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
file(TO_CMAKE_PATH "${PAYLOAD_ZIP}" PAYLOAD_FILE)
file(TO_CMAKE_PATH "${ROOT_DIR}/resources/app.ico" ICON_FILE)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/setup.manifest" MANIFEST_FILE)
set(UNINSTALLER_FILE "${CMAKE_CURRENT_BINARY_DIR}/Uninstall.exe")

set(WIN_LIBS comctl32 shell32 ole32 uuid gdi32 user32 advapi32 msimg32)
set(STATIC_FLAGS -static -static-libgcc -static-libstdc++ -municode)

# --- Uninstall.exe (без архіву)
configure_file(uninstall.rc.in ${CMAKE_CURRENT_BINARY_DIR}/uninstall.rc @ONLY)
add_executable(Uninstall WIN32 setup.cpp ${CMAKE_CURRENT_BINARY_DIR}/uninstall.rc)
target_compile_definitions(Uninstall PRIVATE LUMIO_UNINSTALLER UNICODE _UNICODE)
target_include_directories(Uninstall PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Uninstall PRIVATE ${WIN_LIBS})
target_link_options(Uninstall PRIVATE ${STATIC_FLAGS})
set_target_properties(Uninstall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# --- LumioPhoto-Setup.exe (з архівом програми та деінсталятором усередині)
configure_file(setup.rc.in ${CMAKE_CURRENT_BINARY_DIR}/setup.rc @ONLY)
add_executable(LumioSetup WIN32 setup.cpp ${ROOT_DIR}/third_party/miniz/miniz.c ${CMAKE_CURRENT_BINARY_DIR}/setup.rc)
target_compile_definitions(LumioSetup PRIVATE UNICODE _UNICODE)
target_include_directories(LumioSetup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${ROOT_DIR}/third_party/miniz)
target_link_libraries(LumioSetup PRIVATE ${WIN_LIBS})
target_link_options(LumioSetup PRIVATE ${STATIC_FLAGS})
set_target_properties(LumioSetup PROPERTIES OUTPUT_NAME "LumioPhoto-Setup" RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(LumioSetup Uninstall)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/setup.rc PROPERTIES
    OBJECT_DEPENDS "${UNINSTALLER_FILE};${PAYLOAD_FILE};${ICON_FILE}")
