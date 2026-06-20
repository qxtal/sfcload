AM_CFLAGS = -Wall -Wextra -std=c99 $(LIBSERIALPORT_CFLAGS)
bin_PROGRAMS = sfcload

sfcload_SOURCES = \
	src/compat/memmem.c \
    src/crc32.c \
    src/debug.c \
    src/file.c \
    src/packet.c \
    src/sd2.c \
    src/serial.c \
    src/sfcload.c \
    src/common.h \
    src/compat/memmem.h \
    src/crc32.h \
    src/debug.h \
    src/file.h \
    src/packet.h \
    src/sd2.h \
    src/serial.h

sfcload_LDADD = $(LIBSERIALPORT_LIBS)

dist_noinst_DATA = \
    src/deps/dmon/dmon.h \
    src/deps/dmon/LICENSE