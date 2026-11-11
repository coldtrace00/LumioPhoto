cmake_minimum_required(VERSION 3.16)

project(LumioPhoto VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt Network використовується лише вікном «Перевірити оновлення» (тільки за натисканням користувача).
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql PrintSupport Network)

if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION VERSION_LESS 5.15)
    message(FATAL_ERROR "Потрібен Qt 5.15 або новіший")
endif()

set(SOURCES
    src/main.cpp
    src/photo.h
    src/exif.h            src/exif.cpp
    src/edits.h           src/edits.cpp
    src/library.h         src/library.cpp
    src/scanner.h         src/scanner.cpp
    src/thumbcache.h      src/thumbcache.cpp
    src/util.h            src/util.cpp
    src/icons.h           src/icons.cpp
    src/photomodel.h      src/photomodel.cpp
    src/imagecanvas.h     src/imagecanvas.cpp
    src/infopanel.h       src/infopanel.cpp
    src/viewerpage.h      src/viewerpage.cpp
    src/slideshow.h       src/slideshow.cpp
    src/exportdialog.h    src/exportdialog.cpp
    src/mainwindow.h      src/mainwindow.cpp
    src/theme.h           src/theme.cpp
    src/i18n.h            src/i18n.cpp
    src/updater.h         src/updater.cpp
    src/quickviewer.h     src/quickviewer.cpp
    src/fileassoc.h       src/fileassoc.cpp
)

if(WIN32)
    list(APPEND SOURCES resources/app.rc)
endif()

add_executable(LumioPhoto ${SOURCES})
target_compile_definitions(LumioPhoto PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(LumioPhoto PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32)
    target_include_directories(LumioPhoto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    target_link_libraries(LumioPhoto PRIVATE user32 shell32 advapi32)
    set_target_properties(LumioPhoto PROPERTIES WIN32_EXECUTABLE ON)
endif()

if(MSVC)
    target_compile_options(LumioPhoto PRIVATE /utf-8)
endif()
