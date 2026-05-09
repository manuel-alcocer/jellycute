cmake_minimum_required(VERSION 3.20)
project(jellycute CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Network OpenGL Qml Quick QuickControls2 Widgets)
find_package(KF6Kirigami REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

add_executable(jellycute
    src/main.cpp
    src/JellyfinClient.cpp
    src/AccountStore.cpp
    src/ItemListModel.cpp
    src/BrowseModel.cpp
    src/ItemDetailModel.cpp
    src/PlaybackSession.cpp
    src/MpvObject.cpp
    src/Preferences.cpp
    resources.qrc
)

target_include_directories(jellycute PRIVATE src)
target_link_libraries(jellycute PRIVATE
    Qt6::Network
    Qt6::OpenGL
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::Kirigami
    PkgConfig::MPV
)
