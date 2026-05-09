cmake_minimum_required(VERSION 3.20)
project(jellycute CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network OpenGLWidgets Qml Quick QuickControls2)
find_package(KF6Kirigami REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

add_executable(jellycute
    src/main.cpp
    src/MainWindow.cpp
    src/LoginDialog.cpp
    src/JellyfinClient.cpp
    src/BrowserWidget.cpp
    src/DetailPage.cpp
    src/MediaRow.cpp
    src/MpvWidget.cpp
    src/SettingsDialog.cpp
    src/Theme.cpp
    src/AccountStore.cpp
    src/ServerProbe.cpp
    src/ItemListModel.cpp
    src/BrowseModel.cpp
    src/MpvObject.cpp
    resources.qrc
)

target_include_directories(jellycute PRIVATE src)
target_link_libraries(jellycute PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGLWidgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::Kirigami
    PkgConfig::MPV
)
