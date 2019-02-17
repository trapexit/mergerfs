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
FUSE_LIBS   = libfuse/obj/libfuse.a
FUSE_TARGET = $(FUSE_LIBS)

ifeq ($(STATIC),1)
STATIC_FLAG := -static
else
STATIC_FLAG :=
endif

ifeq ($(LTO),1)
LTO_FLAG := -flto
else
LTO_FLAG :=
endif

UGID_USE_RWLOCK = 0

OPTS 	    = -O2 -g
SRC	    = $(wildcard src/*.cpp)
OBJ         = $(SRC:src/%.cpp=obj/%.o)
DEPS        = $(OBJ:obj/%.o=obj/%.d)
TARGET      = mergerfs
MANPAGE     = $(TARGET).1
CXXFLAGS    = $(OPTS) \
              $(STATIC_FLAG) \
              $(LTO_FLAG) \
              -Wall \
	      -Wno-unused-result \
              $(FUSE_CFLAGS) \
              -DFUSE_USE_VERSION=29 \
              -MMD \
	      -DUSE_XATTR=$(USE_XATTR) \
	      -DUGID_USE_RWLOCK=$(UGID_USE_RWLOCK)

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

all: $(TARGET)

help:
	@echo "usage: make\n"
	@echo "make USE_XATTR=0      - build program without xattrs functionality"
	@echo "make STATIC=1         - build static binary"
	@echo "make LTO=1            - build with link time optimization"

$(TARGET): version obj/obj-stamp $(FUSE_TARGET) $(OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(OBJ) -o $@ $(FUSE_LIBS) -ldl -pthread -lrt

mount.mergerfs: $(TARGET)
	$(LN) -fs "$<" "$@"

changelog:
ifeq ($(GIT_REPO),1)
	$(GIT2DEBCL) --name $(TARGET) > ChangeLog
else
	@echo "WARNING: need git repo to generate ChangeLog"
endif

authors:
ifeq ($(GIT_REPO),1)
	$(GIT) log --format='%aN <%aE>' | sort -f | uniq > AUTHORS
else
	@echo "WARNING: need git repo to generate AUTHORS"
endif

version:
	tools/update-version

obj/obj-stamp:
	$(MKDIR) -p obj
	$(TOUCH) $@

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean: rpm-clean
	$(RM) -f src/version.hpp
	$(RM) -rf obj
	$(RM) -f "$(TARGET)" mount.mergerfs
	$(FIND) . -name "*~" -delete
	cd libfuse && $(MAKE) clean

distclean: clean
ifeq ($(GIT_REPO),1)
	$(GIT) clean -xfd
endif

install: install-base install-mount.mergerfs install-man

install-base: $(TARGET)
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(INSTALL) -v -m 0755 "$(TARGET)" "$(INSTALLBINDIR)/$(TARGET)"

install-mount.mergerfs: mount.mergerfs
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(CP) -a "$<" "$(INSTALLBINDIR)/$<"

install-man: $(MANPAGE)
	$(MKDIR) -p "$(INSTALLMAN1DIR)"
	$(INSTALL) -v -m 0644 "man/$(MANPAGE)" "$(INSTALLMAN1DIR)/$(MANPAGE)"

install-strip: install-base
	$(STRIP) "$(INSTALLBINDIR)/$(TARGET)"

uninstall: uninstall-base uninstall-mount.mergerfs uninstall-man

uninstall-base:
	$(RM) -f "$(INSTALLBINDIR)/$(TARGET)"

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

tarball: superclean man changelog authors version
	$(eval VERSION := $(shell cat VERSION))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(eval FILENAME := $(TARGET)-$(VERSION))
	$(eval TMPDIR := $(shell $(MKTEMP) --tmpdir -d .$(FILENAME).XXXXXXXX))
	$(MKDIR) $(TMPDIR)/$(FILENAME)
	$(CP) -ar . $(TMPDIR)/$(FILENAME)
	$(TAR) --exclude=.git -cz -C $(TMPDIR) -f $(FILENAME).tar.gz $(FILENAME)
	$(RM) -rf $(TMPDIR)

debian-changelog:
ifeq ($(GIT_REPO),1)
	$(GIT2DEBCL) --name $(TARGET) > debian/changelog
else
	cp ChangeLog debian/changelog
endif

signed-deb: debian-changelog
	dpkg-buildpackage

deb: debian-changelog
	dpkg-buildpackage -uc -us

rpm-clean:
	$(RM) -rf rpmbuild

rpm: tarball
	$(eval VERSION := $(shell cat VERSION))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(MKDIR) -p rpmbuild/BUILD rpmbuild/RPMS rpmbuild/SOURCES
	$(SED) 's/__VERSION__/$(VERSION)/g' $(TARGET).spec > \
		rpmbuild/SOURCES/$(TARGET).spec
	cp -ar $(TARGET)-$(VERSION).tar.gz rpmbuild/SOURCES
	$(RPMBUILD) -ba rpmbuild/SOURCES/$(TARGET).spec \
		--define "_topdir $(CURDIR)/rpmbuild"

install-build-pkgs:
	tools/install-build-pkgs

unexport CFLAGS
libfuse/obj/libfuse.a:
	cd libfuse && $(MAKE) libfuse.a

.PHONY: all clean install help version

-include $(DEPS)
