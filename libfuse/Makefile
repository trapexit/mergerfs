VERSION = "2.9.7-mergerfs_2.26.0"
OPT = -O2

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

OBJ  = $(SRC:lib/%.c=obj/%.o)
DEPS = $(OBJ:obj/%.o=obj/%.d)
CFLAGS = $(OPT) \
	 $(DEBUG_FLAGS) \
	 -Wall \
	 -pipe \
	 '-D_FILE_OFFSET_BITS=64' \
	 '-D_REENTRANT' \
	 '-DFUSE_USE_VERSION=29' \
	 '-DFUSERMOUNT_DIR="/usr/local/bin"' \
	 '-DPACKAGE_VERSION=$(VERSION)' \
	 -Iinclude \
	 -MMD
LDFLAGS = \
	-lrt \
	-pthread

all: obj/libfuse.a

libfuse.a: obj/libfuse.a

include/config.h:
	ecfd/build | tee include/config.h

obj/obj-stamp:
	mkdir -p obj
	touch $@

obj/libfuse.a:
	$(MAKE) obj/obj-stamp
	$(MAKE) include/config.h
	$(MAKE) $(OBJ)
	ar rcs obj/libfuse.a $(OBJ)

mergerfs-mount: include/config.h util/fusermount.c lib/mount_util.c
	$(CC) $(CFLAGS) -Ilib -o mergerfs-mount util/fusermount.c lib/mount_util.c

mount.mergerfs: obj/libfuse.a util/mount.fuse.c
	$(CC) $(CFLAGS) -o mount.mergerfs util/mount.fuse.c obj/libfuse.a $(LDFLAGS)

obj/%.o: lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj include/config.h mergerfs-mount mount.mergerfs

distclean: clean
	git clean -fdx

install:
	install -D mergerfs-mount "$(DESTDIR)$(BINDIR)/mergerfs-mount"
	chown root:root "$(DESTDIR)$(BINDIR)/mergerfs-mount"
	chmod u+s "$(DESTDIR)$(BINDIR)/mergerfs-mount"
	install -D mount.mergerfs "$(DESTDIR)$(SBINDIR)/mount.mergerfs"

-include $(DEPS)
