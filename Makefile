#  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>
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

GIT 	  = git
TAR 	  = tar
MKDIR     = mkdir
TOUCH 	  = touch
CP        = cp
RM 	  = rm
LN        = ln
FIND 	  = find
INSTALL   = install
MKTEMP    = mktemp
STRIP     = strip
PANDOC    = pandoc
SED       = sed
RPMBUILD  = rpmbuild
GIT2DEBCL = ./tools/git2debcl
PKGCONFIG = pkg-config

GIT_REPO = 0
ifneq ($(shell $(GIT) --version 2> /dev/null),)
ifeq  ($(shell test -e .git; echo $$?),0)
GIT_REPO = 1
endif
endif

USE_XATTR = 1

FUSE_CFLAGS = -D_FILE_OFFSET_BITS=64 -Ilibfuse/include

ifeq ($(DEBUG),1)
DEBUG_FLAGS := -g
else
DEBUG_FLAGS :=
endif

ifeq ($(STATIC),1)
STATIC_FLAGS := -static
else
STATIC_FLAGS :=
endif

ifeq ($(LTO),1)
LTO_FLAGS := -flto
else
LTO_FLAGS :=
endif

UGID_USE_RWLOCK = 0

OPTS 	    = -O2
SRC	    = $(wildcard src/*.cpp)
OBJS        = $(SRC:src/%.cpp=build/%.o)
DEPS        = $(SRC:src/%.cpp=build/%.d)
MANPAGE     = mergerfs.1
CXXFLAGS   += \
	      $(OPTS) \
              $(DEBUG_FLAGS) \
              $(STATIC_FLAGS) \
              $(LTO_FLAGS) \
              -Wall \
	      -Wno-unused-result \
              $(FUSE_CFLAGS) \
              -DFUSE_USE_VERSION=29 \
              -MMD \
	      -DUSE_XATTR=$(USE_XATTR) \
	      -DUGID_USE_RWLOCK=$(UGID_USE_RWLOCK)
LDFLAGS    += \
	      -pthread \
              -lrt

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

.PHONY: all
all: mergerfs

.PHONY: help
help:
	@echo "usage: make\n"
	@echo "make USE_XATTR=0      - build program without xattrs functionality"
	@echo "make STATIC=1         - build static binary"
	@echo "make LTO=1            - build with link time optimization"

objects: version build/stamp
	$(MAKE) $(OBJS)

build/mergerfs: libfuse objects
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(OBJS) -o $@ libfuse/build/libfuse.a $(LDFLAGS)

mergerfs: build/mergerfs

changelog:
ifeq ($(GIT_REPO),1)
	$(GIT2DEBCL) --name mergerfs > ChangeLog
else
	@echo "WARNING: need git repo to generate ChangeLog"
endif

.PHONY: version
version:
	tools/update-version

build/stamp:
	$(MKDIR) -p build
	$(TOUCH) $@

build/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: clean
clean: rpm-clean
	$(RM) -rf build
	$(FIND) . -name "*~" -delete
	$(MAKE) -C libfuse clean

distclean: clean
ifeq ($(GIT_REPO),1)
	$(GIT) clean -xfd
endif

.PHONY: install
install: install-base install-mount.mergerfs install-man

install-base: build/mergerfs
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(INSTALL) -v -m 0755 build/mergerfs "$(INSTALLBINDIR)/mergerfs"

install-mount.mergerfs: install-base
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(LN) -s "mergerfs" "$(INSTALLBINDIR)/mount.mergerfs"

install-man: $(MANPAGE)
	$(MKDIR) -p "$(INSTALLMAN1DIR)"
	$(INSTALL) -v -m 0644 "man/$(MANPAGE)" "$(INSTALLMAN1DIR)/$(MANPAGE)"

install-strip: install-base
	$(STRIP) "$(INSTALLBINDIR)/mergerfs"

.PHONY: uninstall
uninstall: uninstall-base uninstall-mount.mergerfs uninstall-man

uninstall-base:
	$(RM) -f "$(INSTALLBINDIR)/mergerfs"

uninstall-mount.mergerfs:
	$(RM) -f "$(INSTALLBINDIR)/mount.mergerfs"

uninstall-man:
	$(RM) -f "$(INSTALLMAN1DIR)/$(MANPAGE)"

$(MANPAGE): README.md
ifneq ($(shell $(PANDOC) --version 2> /dev/null),)
	$(PANDOC) -s -t man -o "man/$(MANPAGE)" README.md
else
	$(warning "pandoc does not appear available: unable to build manpage")
endif

man: $(MANPAGE)

.PHONY: tarball
tarball: man changelog version
	$(eval VERSION := $(shell cat VERSION))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(eval FILENAME := mergerfs-$(VERSION))
	$(eval TMPDIR := $(shell $(MKTEMP) --tmpdir -d .$(FILENAME).XXXXXXXX))
	$(MKDIR) $(TMPDIR)/$(FILENAME)
	$(CP) -ar . $(TMPDIR)/$(FILENAME)
	$(TAR) --exclude=.git -cz -C $(TMPDIR) -f $(FILENAME).tar.gz $(FILENAME)
	$(RM) -rf $(TMPDIR)

debian-changelog:
ifeq ($(GIT_REPO),1)
	$(GIT2DEBCL) --name mergerfs > debian/changelog
else
	cp ChangeLog debian/changelog
endif

signed-deb:
	$(MAKE) distclean
	$(MAKE) debian-changelog
	dpkg-source -b .
	dpkg-buildpackage -nc

deb:
	$(MAKE) distclean
	$(MAKE) debian-changelog
	dpkg-source -b .
	dpkg-buildpackage -nc -uc -us

.PHONY: rpm-clean
rpm-clean:
	$(RM) -rf rpmbuild

rpm: tarball
	$(eval VERSION := $(shell cat VERSION))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(MKDIR) -p rpmbuild/BUILD rpmbuild/RPMS rpmbuild/SOURCES
	$(SED) 's/__VERSION__/$(VERSION)/g' mergerfs.spec > \
		rpmbuild/SOURCES/mergerfs.spec
	cp -ar mergerfs-$(VERSION).tar.gz rpmbuild/SOURCES
	$(RPMBUILD) -ba rpmbuild/SOURCES/mergerfs.spec \
		--define "_topdir $(CURDIR)/rpmbuild"

.PHONY: install-build-pkgs
install-build-pkgs:
	tools/install-build-pkgs

.PHONY: libfuse
libfuse:
	$(MAKE) -C libfuse

-include $(DEPS)
