TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    Video.cpp \
    Classifier.cpp

LIBS += \
    -lopencv_imgproc \
    -lopencv_video \
    -lopencv_objdetect \
    -lopencv_core \
    -lavutil \
    -lavcodec \
    -lavformat \
    -lswscale \

HEADERS += \
    Video.h \
    Classifier.h
