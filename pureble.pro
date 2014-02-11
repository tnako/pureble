TEMPLATE = lib
CONFIG -= qt
VPATH += ./src
VPATH += ./include

INCLUDEPATH += ./include
DESTDIR = ./bin

CONFIG += debug_and_release build_all staticlib
# CONFIG += staticlib

CONFIG(release, debug|release) {
OBJECTS_DIR = ./obj/release
}

CONFIG(debug, debug|release) {
TARGET = $$join(TARGET,,,d)
OBJECTS_DIR = ./obj/debug
}

QMAKE_LINK = gcc
QMAKE_CFLAGS += -std=gnu11 -Wextra -Werror -DMODULE_NAME=\\\"\$\$\(p=\"$@\"; echo \$\${p%.*}\)\\\"

QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector -march=native -mtune=native

QMAKE_LFLAGS += -Wl,--as-needed

#--- 3d party
# tommyds
VPATH += ./third_party/tommyds
INCLUDEPATH += ./third_party/tommyds


SOURCES += \
    src/plog/log.c \
    src/papp/args.c \
    src/papp/help.c \
    src/pcore/hash.c \
    third_party/tommyds/tommy.c

HEADERS += \
    include/plog/log.h \
    include/papp/args.h \
    include/papp/help.h \
    include/pcore/hash.h \
    third_party/tommyds/tommy.h \
    include/pcore/types.h \
    include/pureble.h \
    include/pcore/internal.h

