# AiChat.pro - qmake build file
QT += quick network
CONFIG += c++14
TEMPLATE = app
TARGET = AiChat

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    AiController.cpp \
    PromptManager.cpp \
    FileAgent.cpp \
    CommandRunner.cpp \
    ChatManager.cpp

HEADERS += \
    AiController.h \
    PromptManager.h \
    FileAgent.h \
    CommandRunner.h \
    ChatManager.h

RESOURCES += \
    qml.qrc
