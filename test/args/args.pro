TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../../include
QMAKE_LIBDIR += ../../bin
QMAKE_RPATHDIR += ../../bin

CONFIG(release, debug|release) {
QMAKE_POST_LINK = strip -s $(TARGET)
LIBS += -lpureble
}

CONFIG(debug, debug|release) {
TARGET = $$join(TARGET,,,d)
LIBS += -lpurebled
}

QMAKE_LINK = gcc
QMAKE_CFLAGS += -std=gnu11 -Wextra -Werror -DPACKAGE_NAME=\\\"$(QMAKE_TARGET)\\\" -DMODULE_NAME=\\\"\$\$\(p=\"$@\"; p=\$\${p\\$$LITERAL_HASH\\$$LITERAL_HASH*/}; echo \$\${p%.*}\)\\\"

QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector -march=native -mtune=native

QMAKE_LFLAGS += -Wl,--as-needed

SOURCES += main.c

