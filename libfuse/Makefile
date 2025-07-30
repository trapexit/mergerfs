AR      ?= ar
CHMOD   ?= chmod
CHOWN   ?= chown
INSTALL ?= install
MKDIR	?= mkdir
RM      ?= rm
STRIP	?= strip
TOUCH	?= touch

OS := $(shell uname -s)
ifeq ($(OS),Linux)
UTILS := utils
INSTALLUTILS := install-utils
else
UTILS :=
INSTALLUTILS :=
endif

ifeq ($(NDEBUG),1)
OPT_FLAGS := -O2 -DNDEBUG
else
OPT_FLAGS := -O0 -g -DDEBUG -fno-omit-frame-pointer
endif

ifeq ($(LTO),1)
LTO_FLAGS := -flto
else
LTO_FLAGS :=
endif

# https://www.gnu.org/prep/standards/html_node/Directory-Variables.html
DESTDIR       ?=
PREFIX        ?= /usr/local
EXEC_PREFIX   ?= $(PREFIX)
DATAROOTDIR   ?= $(PREFIX)/share
DATADIR       ?= $(DATAROOTDIR)
BINDIR        ?= $(EXEC_PREFIX)/bin
SBINDIR       ?= /sbin
MANDIR        ?= $(DATAROOTDIR)/man
MAN1DIR       ?= $(MANDIR)/man1

INSTALLBINDIR  ?= $(DESTDIR)$(BINDIR)
INSTALLSBINDIR ?= $(DESTDIR)$(SBINDIR)
INSTALLMAN1DIR ?= $(DESTDIR)$(MAN1DIR)

BUILDDIR := build

SRC_C = \
	lib/buffer.c \
	lib/crc32b.c \
	lib/debug.c \
	lib/fuse_dirents.c \
	lib/fuse_lowlevel.c \
	lib/fuse_opt.c \
	lib/fuse_session.c \
	lib/fuse_signals.c \
	lib/helper.c \
	lib/mount.c
SRC_CXX = \
	lib/fuse.cpp \
	lib/cpu.cpp \
	lib/fuse_config.cpp \
	lib/fuse_loop.cpp \
	lib/fuse_msgbuf.cpp \
	lib/pin_threads.cpp \
	lib/format.cpp \
	lib/node.cpp \
	lib/maintenance_thread.cpp

OBJS_C   = $(SRC_C:lib/%.c=$(BUILDDIR)/%.o)
OBJS_CXX = $(SRC_CXX:lib/%.cpp=$(BUILDDIR)/%.o)
DEPS_C   = $(SRC_C:lib/%.c=$(BUILDDIR)/%.d)
DEPS_CXX = $(SRC_CXX:lib/%.cpp=$(BUILDDIR)/%.d)

CPPFLAGS ?=
CFLAGS ?= \
	$(OPT_FLAGS) \
	$(LTO_FLAGS) \
	-Wall \
	-pipe
override CFLAGS += \
	-std=gnu99 \
	-MMD \
	-MP
CXXFLAGS ?= \
	$(OPT_FLAGS) \
	$(LTO_FLAGS) \
	-Wall \
	-pipe
override CXXFLAGS += \
	-std=c++17 \
	-MMD \
	-MP
FUSERMOUNT_DIR := $(BINDIR)
FUSE_FLAGS := \
	-Iinclude \
	-I$(BUILDDIR) \
	-D_REENTRANT \
	-D_FILE_OFFSET_BITS=64 \
	-DFUSERMOUNT_DIR=\"$(FUSERMOUNT_DIR)\"
LDFLAGS ?=
LDLIBS := \
	-lrt \
	-latomic \
	-pthread

all: $(BUILDDIR)/libfuse.a $(UTILS)

$(BUILDDIR)/stamp:
	$(MKDIR) -p $(BUILDDIR)
	$(TOUCH) $@

$(BUILDDIR)/libfuse.a: $(OBJS_C) $(OBJS_CXX)
	$(AR) rcs $(BUILDDIR)/libfuse.a $(OBJS_C) $(OBJS_CXX)

.PHONY: utils
utils: mergerfs-fusermount mount.mergerfs

$(BUILDDIR)/mergerfs-fusermount: util/fusermount.c lib/mount_util.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -Ilib -o $(BUILDDIR)/mergerfs-fusermount util/fusermount.c lib/mount_util.c

mergerfs-fusermount: $(BUILDDIR)/mergerfs-fusermount

$(BUILDDIR)/mount.mergerfs: $(BUILDDIR)/libfuse.a util/mount.mergerfs.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -o $(BUILDDIR)/mount.mergerfs util/mount.mergerfs.c $(BUILDDIR)/libfuse.a $(LDFLAGS)

mount.mergerfs: $(BUILDDIR)/mount.mergerfs

$(BUILDDIR)/%.o: lib/%.c | $(BUILDDIR)/stamp
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -c $< -o $@

$(BUILDDIR)/%.o: lib/%.cpp | $(BUILDDIR)/stamp
	$(CXX) $(CXXFLAGS) $(FUSE_FLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) -rf $(BUILDDIR)

.PHONY: distclean
distclean: clean

.PHONY: strip
strip:
	$(STRIP) --strip-all $(BUILDDIR)/mount.mergerfs
	$(STRIP) --strip-all $(BUILDDIR)/mergerfs-fusermount

install-utils: mergerfs-fusermount mount.mergerfs strip
	$(INSTALL) -D $(BUILDDIR)/mergerfs-fusermount "$(INSTALLBINDIR)/mergerfs-fusermount"
	$(INSTALL) -D $(BUILDDIR)/mount.mergerfs "$(INSTALLSBINDIR)/mount.mergerfs"
	$(CHOWN) root:root "$(INSTALLBINDIR)/mergerfs-fusermount"
	$(CHMOD) u+s "$(INSTALLBINDIR)/mergerfs-fusermount"

install: $(INSTALLUTILS)

.PHONY: objects strip utils install install-utils

-include $(DEPS_C) $(DEPS_CXX)
