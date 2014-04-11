TEMPLATE = lib
CONFIG -= qt
VPATH += ./src
VPATH += ./include

INCLUDEPATH += ./include
DESTDIR = ./bin
OUT_PWD = ./bin

CONFIG += staticlib

CONFIG(release, debug|release) {
    OBJECTS_DIR = ./build/release
    MOC_DIR = ./build/release
    RCC_DIR = ./build/release
    UI_DIR = ./build/release
    INCLUDEPATH += ./build/release
}

CONFIG(debug, debug|release) {
    TARGET = $$join(TARGET,,,d)
    OBJECTS_DIR = ./build/debug
    MOC_DIR = ./build/debug
    RCC_DIR = ./build/debug
    UI_DIR = ./build/debug
    INCLUDEPATH += ./build/debug
}

QMAKE_LINK = gcc
QMAKE_CFLAGS += -std=gnu11 -Wextra -Werror -DMODULE_NAME=\\\"\$\$\(p=\"$@\"; echo \$\${p%.*}\)\\\"

QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector -march=native -mtune=native

QMAKE_LFLAGS += -Wl,--as-needed

#--- 3d party
# tommyds
VPATH += ./third_party/tommyds
INCLUDEPATH += ./third_party/tommyds

VPATH += ./third_party/build/include
INCLUDEPATH += ./third_party/build/include
QMAKE_LIBDIR += ./third_party/build/lib

QMAKE_POST_LINK = cp third_party/build/lib/*.a bin/

SOURCES += \
    src/plog/log.c \
    src/papp/args.c \
    src/papp/help.c \
    src/pcore/hash.c \
    third_party/tommyds/tommy.c \
    src/papp/runner.c \
    src/pobj/object.c \
    src/pobj/timer.c \
    src/pobj/signals.c \
    src/pnet/network.c

HEADERS += \
    include/plog/log.h \
    include/papp/args.h \
    include/papp/help.h \
    include/pcore/hash.h \
    third_party/tommyds/tommy.h \
    include/pcore/types.h \
    include/pureble.h \
    include/pcore/internal.h \
    include/papp/runner.h \
    include/pobj/object.h \
    include/pobj/timer.h \
    include/pobj/signals.h \
    include/pnet/network.h \
    third_party/nanomsg/include/nanomsg/tcp.h \
    third_party/nanomsg/include/nanomsg/survey.h \
    third_party/nanomsg/include/nanomsg/reqrep.h \
    third_party/nanomsg/include/nanomsg/pubsub.h \
    third_party/nanomsg/include/nanomsg/pipeline.h \
    third_party/nanomsg/include/nanomsg/pair.h \
    third_party/nanomsg/include/nanomsg/nn.h \
    third_party/nanomsg/include/nanomsg/ipc.h \
    third_party/nanomsg/include/nanomsg/inproc.h \
    third_party/nanomsg/include/nanomsg/bus.h

