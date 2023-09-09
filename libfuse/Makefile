VERSION = 2.9.7-mergerfs_2.30.0

OS := $(shell uname -s)
ifeq ($(OS),Linux)
UTILS := utils
INSTALLUTILS := install-utils
else
UTILS :=
INSTALLUTILS :=
endif

ifeq ($(DEBUG),1)
OPT_FLAGS := -O0 -g -fsanitize=undefined
else
OPT_FLAGS := -O2
endif

ifeq ($(LTO),1)
LTO_FLAGS := -flto
else
LTO_FLAGS :=
endif

DESTDIR       =
PREFIX        = /usr/local
EXEC_PREFIX   = $(PREFIX)
DATAROOTDIR   = $(PREFIX)/share
DATADIR       = $(DATAROOTDIR)
BINDIR        = $(EXEC_PREFIX)/bin
SBINDIR       = /sbin
MANDIR        = $(DATAROOTDIR)/man
MAN1DIR       = $(MANDIR)/man1

INSTALLBINDIR  = $(DESTDIR)$(BINDIR)
INSTALLSBINDIR = $(DESTDIR)$(SBINDIR)
INSTALLMAN1DIR = $(DESTDIR)$(MAN1DIR)

AR ?= ar

SRC_C = \
	lib/buffer.c \
	lib/crc32b.c \
	lib/debug.c \
	lib/fuse.c \
	lib/fuse_dirents.c \
	lib/fuse_lowlevel.c \
	lib/node.c \
	lib/fuse_node.c \
	lib/fuse_opt.c \
	lib/fuse_session.c \
	lib/fuse_signals.c \
	lib/helper.c \
	lib/mount.c
SRC_CPP = \
	lib/format.cpp \
	lib/os.cpp \
	lib/cpu.cpp \
	lib/fuse_config.cpp \
	lib/fuse_loop.cpp \
	lib/fuse_msgbuf.cpp
OBJS_C   = $(SRC_C:lib/%.c=build/%.o)
OBJS_CPP = $(SRC_CPP:lib/%.cpp=build/%.o)
DEPS_C   = $(SRC_C:lib/%.c=build/%.d)
DEPS_CPP = $(SRC_CPP:lib/%.cpp=build/%.d)
CFLAGS ?= \
	$(OPT_FLAGS)
CFLAGS := \
	${CFLAGS} \
	$(LTO_FLAGS) \
	-std=gnu99 \
	-Wall \
	-pipe \
	-MMD
CXXFLAGS := \
	${CXXFLAGS} \
	$(LTO_FLAGS) \
	-std=c++11 \
	-Wall \
	-pipe \
	-MMD
FUSERMOUNT_DIR = $(BINDIR)
FUSE_FLAGS = \
	-Iinclude \
	-Ibuild \
	-D_REENTRANT \
	-D_FILE_OFFSET_BITS=64 \
	-DPACKAGE_VERSION=\"$(VERSION)\" \
	-DFUSERMOUNT_DIR=\"$(FUSERMOUNT_DIR)\"
LDFLAGS := \
	${LDFLAGS} \
	-lrt \
	-pthread

all: build/libfuse.a $(UTILS)

build/config.h: build/stamp
	ecfd/build | tee build/config.h

build/stamp:
	mkdir -p build
	touch $@

objects: build/config.h
	$(MAKE) $(OBJS_C) $(OBJS_CPP)

build/libfuse.a: objects
	${AR} rcs build/libfuse.a $(OBJS_C) $(OBJS_CPP)

utils: mergerfs-fusermount mount.mergerfs

build/mergerfs-fusermount: build/config.h util/fusermount.c lib/mount_util.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -Ilib -o build/mergerfs-fusermount util/fusermount.c lib/mount_util.c

mergerfs-fusermount: build/mergerfs-fusermount

build/mount.mergerfs: build/libfuse.a util/mount.mergerfs.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -o build/mount.mergerfs util/mount.mergerfs.c build/libfuse.a $(LDFLAGS)

mount.mergerfs: build/mount.mergerfs

build/%.o: lib/%.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -c $< -o $@

build/%.o: lib/%.cpp
	$(CXX) $(CXXFLAGS) $(FUSE_FLAGS) -c $< -o $@

clean:
	rm -rf build

distclean: clean

strip:
	strip --strip-all build/mount.mergerfs
	strip --strip-all build/mergerfs-fusermount

install-utils: mergerfs-fusermount mount.mergerfs strip
	install -D build/mergerfs-fusermount "$(INSTALLBINDIR)/mergerfs-fusermount"
	install -D build/mount.mergerfs "$(INSTALLSBINDIR)/mount.mergerfs"
	chown root "$(INSTALLBINDIR)/mergerfs-fusermount"
	chmod u+s "$(INSTALLBINDIR)/mergerfs-fusermount"

install: $(INSTALLUTILS)

.PHONY: objects strip utils install install-utils

-include $(DEPS_C) $(DEPS_CPP)
