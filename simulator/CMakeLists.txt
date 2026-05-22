cmake_minimum_required(VERSION 3.16)
project(PwmSimulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(PwmSimulator
    main.cpp
    mainwindow.cpp
    oledwidget.cpp
    menuengine.cpp
    serialcomm.cpp
    protocol.cpp
    keymap.cpp
)

target_include_directories(PwmSimulator PRIVATE
    ${CMAKE_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(PwmSimulator PRIVATE
    Qt6::Widgets
    Qt6::SerialPort
)
