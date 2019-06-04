VERSION = "2.9.7-mergerfs_2.28.0"
OPTS = -O2

ifeq ($(DEBUG),1)
DEBUG_FLAGS := -g
else
DEBUG_FLAGS :=
endif

DESTDIR = "/"
BINDIR = "/bin"
SBINDIR = "/sbin"
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

CFLAGS += \
	$(OPTS) \
	$(DEBUG_FLAGS) \
	-Wall \
	-pipe \
	'-D_FILE_OFFSET_BITS=64' \
	'-D_REENTRANT' \
	'-DFUSE_USE_VERSION=29' \
	'-DFUSERMOUNT_DIR="/usr/local/bin"' \
	'-DPACKAGE_VERSION=$(VERSION)' \
	-Iinclude \
	-Ibuild \
	-MMD
LDFLAGS += \
	-lrt \
	-pthread

all: build/libfuse.a

build/config.h: build/stamp
	ecfd/build | tee build/config.h

build/stamp:
	mkdir -p build
	touch $@

objects: build/config.h
	$(MAKE) $(OBJS)

build/libfuse.a: objects
	ar rcs build/libfuse.a $(OBJS)

build/mergerfs-mount: build/config.h util/fusermount.c lib/mount_util.c
	$(CC) $(CFLAGS) -Ilib -o mergerfs-mount util/fusermount.c lib/mount_util.c

build/mount.mergerfs: build/libfuse.a util/mount.fuse.c
	$(CC) $(CFLAGS) -o mount.mergerfs util/mount.fuse.c build/libfuse.a $(LDFLAGS)

build/%.o: lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

distclean: clean

install: build/mergerfs-mount build/mount.mergerfs
	install -D build/mergerfs-mount "$(DESTDIR)$(BINDIR)/mergerfs-mount"
	chown root:root "$(DESTDIR)$(BINDIR)/mergerfs-mount"
	chmod u+s "$(DESTDIR)$(BINDIR)/mergerfs-mount"
	install -D build/mount.mergerfs "$(DESTDIR)$(SBINDIR)/mount.mergerfs"

.PHONY: objects

-include $(DEPS)
