cmake_minimum_required(VERSION 3.16)
project(HeroDock VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)

set(SOURCES
    src/main.cpp
    src/x11util.cpp
    src/AppMenu.cpp
    src/NetworkPanel.cpp
    src/NotificationManager.cpp
    src/SystemPanel.cpp
    src/DeviceManager.cpp
    src/DockDeviceItem.cpp
    src/DockWindow.cpp
    src/DockItem.cpp
    src/ApplicationScanner.cpp
    src/SettingsDialog.cpp
    src/AppLauncher.cpp
    src/DockAnimation.cpp
    src/SystemTrayArea.cpp
    src/TrashItem.cpp
    src/SeparatorItem.cpp
)

set(HEADERS
    src/DockWindow.h
    src/DockItem.h
    src/ApplicationScanner.h
    src/SettingsDialog.h
    src/DockConfig.h
    src/x11_helper.h
    src/AppLauncher.h
    src/DockAnimation.h
    src/SystemTrayArea.h
    src/TrashItem.h
    src/SeparatorItem.h
)

add_executable(herodock ${SOURCES} ${HEADERS})

target_link_libraries(herodock Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus ${X11_LIBRARIES})

target_include_directories(herodock PRIVATE src ${X11_INCLUDE_DIRS})

# Qt6 private headers for QPA native interface (X11 display access)
get_target_property(Qt6_GuiIncludes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir ${Qt6_GuiIncludes})
    if(EXISTS "${dir}/${Qt6_VERSION}/QtGui")
        target_include_directories(herodock PRIVATE "${dir}/${Qt6_VERSION}/QtGui")
    endif()
endforeach()

install(TARGETS herodock DESTINATION bin)
install(FILES herodock.desktop DESTINATION share/applications)
