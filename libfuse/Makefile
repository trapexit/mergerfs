#  Copyright (c) 2025, Antonio SJ Musumeci <trapexit@spawn.link>
#
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
#
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

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
	lib/fuse_dirents.c \
	lib/fuse_opt.c \
	lib/fuse_session.c \
	lib/fuse_signals.c \
	lib/helper.c \
	lib/mount.c
SRC_CXX = $(wildcard lib/*.cpp)

OBJS := $(SRC_C:lib/%.c=$(BUILDDIR)/.objs/%.c.o)
OBJS += $(SRC_CXX:lib/%.cpp=$(BUILDDIR)/.objs/%.cpp.o)
DEPS := $(SRC_C:lib/%.c=$(BUILDDIR)/.objs/%.c.d)
DEPS += $(SRC_CXX:lib/%.cpp=$(BUILDDIR)/.objs/%.cpp.d)

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
	$(MKDIR) -p $(BUILDDIR)/.objs/
	$(TOUCH) $@

$(BUILDDIR)/libfuse.a: $(OBJS)
	$(AR) rcs $(BUILDDIR)/libfuse.a $(OBJS)

.PHONY: utils
utils: mergerfs-fusermount mount.mergerfs

$(BUILDDIR)/mergerfs-fusermount: util/fusermount.c lib/mount_util.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -Ilib -o $(BUILDDIR)/mergerfs-fusermount util/fusermount.c lib/mount_util.c

mergerfs-fusermount: $(BUILDDIR)/mergerfs-fusermount

$(BUILDDIR)/mount.mergerfs: $(BUILDDIR)/libfuse.a util/mount.mergerfs.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -o $(BUILDDIR)/mount.mergerfs util/mount.mergerfs.c $(BUILDDIR)/libfuse.a $(LDFLAGS)

mount.mergerfs: $(BUILDDIR)/mount.mergerfs

$(BUILDDIR)/.objs/%.c.o: lib/%.c | $(BUILDDIR)/stamp
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -c $< -o $@

$(BUILDDIR)/.objs/%.cpp.o: lib/%.cpp | $(BUILDDIR)/stamp
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

-include $(DEPS)
