cmake_minimum_required(VERSION 3.14)
project(AiChat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ===== Qt 5.14 =====
find_package(Qt5 REQUIRED COMPONENTS Quick Network)

add_executable(AiChat
    main.cpp
    AiController.h
    AiController.cpp
    PromptManager.h
    PromptManager.cpp
    main.qml
    qml.qrc
)

target_link_libraries(AiChat PRIVATE
    Qt5::Quick
    Qt5::Network
)
