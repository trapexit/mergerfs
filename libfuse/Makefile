VERSION = 2.9.7-mergerfs_2.29.0

ifeq ($(DEBUG),1)
DEBUG_FLAGS := -g
else
DEBUG_FLAGS :=
endif

DESTDIR       =
PREFIX        = /usr/local
EXEC_PREFIX   = $(PREFIX)
DATAROOTDIR   = $(PREFIX)/share
DATADIR       = $(DATAROOTDIR)
BINDIR        = $(EXEC_PREFIX)/bin
SBINDIR       = $(EXEC_PREFIX)/sbin
MANDIR        = $(DATAROOTDIR)/man
MAN1DIR       = $(MANDIR)/man1

INSTALLBINDIR  = $(DESTDIR)$(BINDIR)
INSTALLSBINDIR = $(DESTDIR)$(SBINDIR)
INSTALLMAN1DIR = $(DESTDIR)$(MAN1DIR)

SRC   = \
	lib/buffer.c \
	lib/cuse_lowlevel.c \
	lib/fuse.c \
	lib/fuse_kern_chan.c \
	lib/fuse_loop.c \
	lib/fuse_loop_mt.c \
	lib/fuse_lowlevel.c \
	lib/fuse_mt.c \
	lib/fuse_opt.c \
	lib/fuse_session.c \
	lib/fuse_signals.c \
	lib/helper.c \
	lib/mount.c
OBJS = $(SRC:lib/%.c=build/%.o)
DEPS = $(SRC:lib/%.c=build/%.d)

CFLAGS = \
	-O2 \
	$(DEBUG_FLAGS) \
	-Wall \
	-pipe \
	-MMD
FUSERMOUNT_DIR = $(BINDIR)
FUSE_FLAGS = \
	-Iinclude \
	-Ibuild \
	-D_REENTRANT \
	-D_FILE_OFFSET_BITS=64 \
	-DFUSE_USE_VERSION=29 \
	-DPACKAGE_VERSION=\"$(VERSION)\" \
	-DFUSERMOUNT_DIR=\"$(FUSERMOUNT_DIR)\"
LDFLAGS = \
	-lrt \
	-pthread

all: build/libfuse.a mount.mergerfs mergerfs-fusermount

build/config.h: build/stamp
	ecfd/build | tee build/config.h

build/stamp:
	mkdir -p build
	touch $@

objects: build/config.h
	$(MAKE) $(OBJS)

build/libfuse.a: objects
	ar rcs build/libfuse.a $(OBJS)

build/mergerfs-fusermount: build/config.h util/fusermount.c lib/mount_util.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -Ilib -o build/mergerfs-fusermount util/fusermount.c lib/mount_util.c

mergerfs-fusermount: build/mergerfs-fusermount

build/mount.mergerfs: build/libfuse.a util/mount.mergerfs.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -o build/mount.mergerfs util/mount.mergerfs.c build/libfuse.a $(LDFLAGS)

mount.mergerfs: build/mount.mergerfs

build/%.o: lib/%.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -c $< -o $@

clean:
	rm -rf build

distclean: clean

strip:
	strip --strip-all build/mount.mergerfs
	strip --strip-all build/mergerfs-fusermount

install: mergerfs-fusermount mount.mergerfs strip
	install -D build/mergerfs-fusermount "$(INSTALLBINDIR)/mergerfs-fusermount"
	install -D build/mount.mergerfs "$(INSTALLSBINDIR)/mount.mergerfs"
	chown root:root "$(INSTALLBINDIR)/mergerfs-fusermount"
	chmod u+s "$(INSTALLBINDIR)/mergerfs-fusermount"

.PHONY: objects strip

-include $(DEPS)
