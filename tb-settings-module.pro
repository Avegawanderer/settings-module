TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.c \
        settings.c \
        settings_private.c \
        utils.c

HEADERS += \
    settings.h \
    settings_private.h \
    settings_public.h \
    utils.h
