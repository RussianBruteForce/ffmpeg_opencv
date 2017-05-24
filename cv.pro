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

#    -lavutil \
#    -lavcodec \
#    -lavformat \
#    -lswscale \

LIBS += -lpthread \
        /home/abc/ffmpeg_build/lib/libavcodec.so \
        /home/abc/ffmpeg_build/lib/libavdevice.so \
        /home/abc/ffmpeg_build/lib/libavfilter.so \
        /home/abc/ffmpeg_build/lib/libavformat.so \
        /home/abc/ffmpeg_build/lib/libavutil.so \
        /home/abc/ffmpeg_build/lib/libpostproc.so \
        /home/abc/ffmpeg_build/lib/libswresample.so \
        /home/abc/ffmpeg_build/lib/libswscale.so

INCLUDEPATH += "/home/abc/ffmpeg_build/include"

HEADERS += \
    Video.h \
    Classifier.h
