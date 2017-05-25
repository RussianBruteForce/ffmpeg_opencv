TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    Video.cpp \
    Classifier.cpp

HOME = $$system(echo ~)
FFMPEG_BUILD = $$HOME/ffmpeg_build

!exists($$FFMPEG_BUILD):error("Looks like you don't set FFMPEG_BUILD variable")

# ffmpeg
LIBS += -lpthread\
       -lopencv_imgproc \
       -lopencv_video \
       -lopencv_objdetect \
       -lopencv_core \
        $$FFMPEG_BUILD/lib/libavcodec.so \
        $$FFMPEG_BUILD/lib/libavdevice.so \
        $$FFMPEG_BUILD/lib/libavfilter.so \
        $$FFMPEG_BUILD/lib/libavformat.so \
        $$FFMPEG_BUILD/lib/libavutil.so \
        $$FFMPEG_BUILD/lib/libpostproc.so \
        $$FFMPEG_BUILD/lib/libswresample.so \
        $$FFMPEG_BUILD/lib/libswscale.so

INCLUDEPATH += $$FFMPEG_BUILD/include

HEADERS += \
    Video.h \
    Classifier.h
