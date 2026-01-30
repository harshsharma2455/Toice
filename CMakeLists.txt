cmake_minimum_required(VERSION 3.16)
project(Toice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 and X11
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Multimedia Widgets DBus Network Sql Svg)
find_package(X11 REQUIRED)

# Include Directories
include_directories(include)

# Include whisper.cpp (External)
add_subdirectory(external/whisper.cpp)

# Sources
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/overlaywidget.cpp
    src/audiorecorder.cpp
    src/inferenceworker.cpp
    src/globalshortcut.cpp
    resources.qrc
)

set(HEADERS
    include/mainwindow.h
    include/overlaywidget.h
    include/audiorecorder.h
    include/inferenceworker.h
    include/globalshortcut.h
    include/databasemanager.h
    include/setupwizard.h
)

# Executable
add_executable(com.toice.app ${SOURCES} ${HEADERS})

# Link Libraries
target_link_libraries(com.toice.app PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::DBus Qt6::Network Qt6::Sql Qt6::Svg
    ${X11_LIBRARIES}
    whisper
)

# Qt settings
set_target_properties(com.toice.app PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# Install Target (Crucial for Flatpak)
install(TARGETS com.toice.app RUNTIME DESTINATION bin)
install(FILES scripts/toice.sh DESTINATION bin PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(FILES scripts/toice-trigger.sh DESTINATION bin PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(DIRECTORY assets DESTINATION share/toice)

# Install Icons (Standard Hicolor Structure - Critical for KDE)
install(FILES assets/logo.svg DESTINATION share/icons/hicolor/scalable/apps RENAME com.toice.app.svg)
install(FILES assets/icon_32.png DESTINATION share/icons/hicolor/32x32/apps RENAME com.toice.app.png)
install(FILES assets/icon_64.png DESTINATION share/icons/hicolor/64x64/apps RENAME com.toice.app.png)
install(FILES assets/icon_128.png DESTINATION share/icons/hicolor/128x128/apps RENAME com.toice.app.png)
install(FILES assets/icon_256.png DESTINATION share/icons/hicolor/256x256/apps RENAME com.toice.app.png)
install(FILES assets/icon.png DESTINATION share/icons/hicolor/512x512/apps RENAME com.toice.app.png)
