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

ifeq ($(shell id -u),0)
FAKEROOT ?=
endif

CP        ?= cp
FAKEROOT  ?= fakeroot
FIND 	  ?= find
GIT 	  ?= git
GIT2DEBCL ?= buildtools/git2debcl
INSTALL   ?= install
LN        ?= ln
MKDIR     ?= mkdir
MKTEMP    ?= mktemp
MV        ?= mv
RM 	  ?= rm
SED       ?= sed
STRIP     ?= strip
TAR 	  ?= tar
TOUCH 	  ?= touch

BUILDDIR := build
GITHUB_REPO ?= "https://github.com/trapexit/mergerfs"
RELEASE_SAMPLE ?= "debian:13.amd64"

ifndef GIT_REPO
ifneq ($(shell $(GIT) --version 2> /dev/null),)
ifeq  ($(shell test -e .git; echo $$?),0)
GIT_REPO = 1
endif
endif
endif

USE_XATTR ?= 1
UGID_USE_RWLOCK ?= 0

ifdef NDEBUG
OPT_FLAGS := -O2 -DNDEBUG
else
ifdef SANITIZE
ifeq ($(SANITIZE),1)
  override SANITIZE := -fsanitize=address,undefined,leak
else
  override SANITIZE := -fsanitize=$(SANITIZE)
endif
endif
OPT_FLAGS := -O0 \
	     -g \
	     -fno-omit-frame-pointer \
	     $(SANITIZE) \
	     -fstack-protector-strong \
             -Wextra \
	     -Werror \
	     -Wno-unused-parameter \
             -DDEBUG
endif

ifdef STATIC
STATIC_FLAGS := -static
else
STATIC_FLAGS :=
endif

ifdef LTO
LTO_FLAGS := -flto
else
LTO_FLAGS :=
endif

SRC  := $(wildcard src/*.cpp)
OBJS := $(SRC:src/%.cpp=build/.objs/%.cpp.o)
DEPS := $(SRC:src/%.cpp=build/.objs/%.cpp.d)

TESTS      := $(wildcard tests/*.cpp)
TESTS_OBJS := $(filter-out build/.objs/mergerfs.cpp.o,$(OBJS))
TESTS_OBJS += $(TESTS:tests/%.cpp=build/.test_objs/%.cpp.o)
TESTS_DEPS := $(TESTS:tests/%.cpp=build/.test_objs/%.cpp.d)
TESTS_DEPS += $(DEPS)

MANPAGE := mergerfs.1
CPPFLAGS ?=
override CPPFLAGS += \
	-D_FILE_OFFSET_BITS=64
CFLAGS ?= \
	$(OPT_FLAGS) \
	-Wall \
	-Wno-unused-result \
	-pipe
override CFLAGS += \
	-MMD \
	-MP
CXXFLAGS ?= \
	$(OPT_FLAGS) \
	$(STATIC_FLAGS) \
	$(LTO_FLAGS) \
	-Wall \
	-Wpessimizing-move \
	-Werror=pessimizing-move \
	-Wredundant-move \
	-Werror=redundant-move \
	-Wno-unused-result \
	-pipe
override CXXFLAGS += \
	-std=c++17 \
	-MMD \
	-MP
override INC_FLAGS := \
	-Isrc \
	-Ilibfuse/include
override MFS_FLAGS  := \
	-DUSE_XATTR=$(USE_XATTR) \
	-DUGID_USE_RWLOCK=$(UGID_USE_RWLOCK)
override TESTS_FLAGS := \
	-Isrc \
	-DTESTS

LIBFUSE := libfuse/$(BUILDDIR)/libfuse.a
LDFLAGS ?=
LDLIBS := \
	-lrt \
	-pthread
override LDLIBS += \
	$(LIBFUSE)
ifeq ($(CXX),g++)
    GCC_VERSION := $(shell $(CXX) -dumpversion | cut -f1 -d.)
    ifeq ($(shell test $(GCC_VERSION) -lt 9; echo $$?),0)
        override LDLIBS += -lstdc++fs
    endif
endif

# https://www.gnu.org/prep/standards/html_node/Directory-Variables.html
DESTDIR       ?=
PREFIX        ?= /usr/local
EXEC_PREFIX   ?= $(PREFIX)
DATAROOTDIR   ?= $(PREFIX)/share
DATADIR       ?= $(DATAROOTDIR)
BINDIR        ?= $(EXEC_PREFIX)/bin
SBINDIR       ?= $(EXEC_PREFIX)/sbin
LIBDIR        ?= $(EXEC_PREFIX)/lib
MANDIR        ?= $(DATAROOTDIR)/man
MAN1DIR       ?= $(MANDIR)/man1

INSTALLBINDIR  ?= $(DESTDIR)$(BINDIR)
INSTALLSBINDIR ?= $(DESTDIR)$(SBINDIR)
INSTALLLIBDIR  ?= $(DESTDIR)$(LIBDIR)/mergerfs
INSTALLMAN1DIR ?= $(DESTDIR)$(MAN1DIR)


.PHONY: all
all: libfuse $(BUILDDIR)/mergerfs $(BUILDDIR)/fsck.mergerfs $(BUILDDIR)/mergerfs.collect-info

.PHONY: help
help:
	@echo "usage: make ARG\n"
	@echo "USE_XATTR=0     - build program without xattrs functionality"
	@echo "STATIC=1        - build static binary"
	@echo "LTO=1           - build with link time optimization"
	@echo "CLEANUP=1       - cleanup images between release build process"
	@echo "PKGDIR=/dir/    - location for release build pkgs"
	@echo "GITREF=gitref   - gitref to use for release builds"


$(BUILDDIR)/mergerfs: $(LIBFUSE) src/version.hpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) $(MFS_FLAGS) $(CPPFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILDDIR)/fsck.mergerfs:
	$(LN) -sf "mergerfs" $@

$(BUILDDIR)/mergerfs.collect-info:
	$(LN) -sf "mergerfs" $@

$(BUILDDIR)/tests: $(BUILDDIR)/mergerfs $(TESTS_OBJS)
	$(CXX) $(CXXFLAGS) $(TESTS_FLAGS) $(INC_FLAGS) $(MFS_FLAGS) $(CPPFLAGS) $(TESTS_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

.PHONY: libfuse
$(LIBFUSE):
libfuse:
	$(MAKE) NDEBUG=$(NDEBUG) -C libfuse

tests: $(BUILDDIR)/tests

.PHONY:
changelog:
ifdef GIT_REPO
	$(GIT2DEBCL) --name mergerfs > ChangeLog
else
	@echo "WARNING: need git repo to generate ChangeLog"
endif

.PHONY: version
version: src/version.hpp

src/version.hpp:
	./buildtools/update-version

$(BUILDDIR)/stamp:
	$(MKDIR) -p $(BUILDDIR)/.objs
	$(MKDIR) -p $(BUILDDIR)/.test_objs
	$(TOUCH) $@

$(BUILDDIR)/.objs/%.cpp.o: src/%.cpp $(BUILDDIR)/stamp
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) $(MFS_FLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILDDIR)/.test_objs/%.cpp.o: tests/%.cpp
	$(CXX) $(CXXFLAGS) $(TESTS_FLAGS) $(INC_FLAGS) $(MFS_FLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILDDIR)/preload.so: $(BUILDDIR)/stamp tools/preload.c
	$(CC) -shared -fPIC $(CFLAGS) -o $@ tools/preload.c

preload: $(BUILDDIR)/preload.so

.PHONY: clean
clean: rpm-clean
	$(RM) -rf $(BUILDDIR)
	$(FIND) . -name "*~" -delete
	$(MAKE) -C libfuse clean

.PHONY: distclean
distclean: clean
ifdef GIT_REPO
	$(GIT) clean -xfd
endif

.PHONY: install
install: install-base install-mount-tools install-preload install-man

.PHONY: install-base
install-base: all
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(INSTALL) -v -m 0755 "$(BUILDDIR)/mergerfs" "$(INSTALLBINDIR)/mergerfs"
	$(LN) -fs "mergerfs" "${INSTALLBINDIR}/fsck.mergerfs"
	$(LN) -fs "mergerfs" "${INSTALLBINDIR}/mergerfs.collect-info"

.PHONY: install-mount-tools
install-mount-tools: install-base
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(MAKE) -C libfuse install

.PHONY: install-man
install-man: man/$(MANPAGE)
	$(MKDIR) -p "$(INSTALLMAN1DIR)"
	$(INSTALL) -v -m 0644 "man/$(MANPAGE)" "$(INSTALLMAN1DIR)/$(MANPAGE)"

.PHONY: install-preload
install-preload: preload
	$(MKDIR) -p "$(INSTALLLIBDIR)"
	$(INSTALL) -v -m 444 "$(BUILDDIR)/preload.so" "$(INSTALLLIBDIR)/preload.so"

.PHONY: install-strip
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

.PHONY: tarball
tarball: changelog version
	$(eval VERSION := $(shell cat VERSION))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(eval FILENAME := mergerfs-$(VERSION))
	$(eval TMPDIR := $(shell $(MKTEMP) --tmpdir -d .$(FILENAME).XXXXXXXX))
	$(MKDIR) $(TMPDIR)/$(FILENAME)
	$(CP) -ar . $(TMPDIR)/$(FILENAME)
	$(TAR) --exclude=.git -cz -C $(TMPDIR) -f $(FILENAME).tar.gz $(FILENAME)
	$(RM) -rf $(TMPDIR)

.PHONY: debian-changelog
debian-changelog:
ifdef GIT_REPO
	$(GIT2DEBCL) --name mergerfs > debian/changelog
else
	$(CP) -v ChangeLog debian/changelog
endif

.PHONY: signed-deb
signed-deb:
	$(MAKE) distclean
	$(MAKE) debian-changelog
	$(FAKEROOT) dpkg-buildpackage -nc

.PHONY: deb
deb:
	$(MAKE) distclean
	$(MAKE) debian-changelog
	$(MAKE) version
	$(FAKEROOT) dpkg-buildpackage -nc -uc -us
	$(MKDIR) -p ./$(BUILDDIR)/pkgs/
	$(MV) -v ../mergerfs*deb ./$(BUILDDIR)/pkgs/

.PHONY: rpm-clean
rpm-clean:
	$(RM) -rf rpmbuild

.PHONY: rpm
rpm: tarball
	$(eval VERSION := $(shell cat VERSION))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(MKDIR) -p rpmbuild/BUILD rpmbuild/RPMS rpmbuild/SOURCES
	$(SED) 's/__VERSION__/$(VERSION)/g' mergerfs.spec > \
		rpmbuild/SOURCES/mergerfs.spec
	cp -ar mergerfs-$(VERSION).tar.gz rpmbuild/SOURCES
	rpmbuild -ba rpmbuild/SOURCES/mergerfs.spec \
		--define "_topdir $(CURDIR)/rpmbuild"

.PHONY: install-build-pkgs
install-build-pkgs:
	./buildtools/install-build-pkgs

.PHONY: install-build-tools
install-build-tools:
	./bulidtools/install-build-tools

define build_release
	$(eval GITREF ?= $(shell git describe --exact-match --tags HEAD 2>/dev/null || git symbolic-ref --short HEAD 2>/dev/null || git rev-parse --short HEAD))
	./buildtools/build-release \
		--target=$(1) \
		$(if $(CLEANUP),--cleanup) \
		$(if $(PKGDIR),--pkgdirpath=$(PKGDIR)) \
		--git-repo=$(if $(REMOTE_REPO),$(GITHUB_REPO),".") \
		--branch=$(GITREF)
endef

.PHONY: release
.PHONY: release-amd64 release-arm64 release-armhf release-riscv64
.PHONY: release-sample release-static release-tarball
release:
	$(call build_release,"all")
release-sample:
	$(call build_release,$(RELEASE_SAMPLE))
release-amd64:
	$(call build_release,"amd64")
release-arm64:
	$(call build_release,"arm64")
release-armhf:
	$(call build_release,"armhf")
release-riscv64:
	$(call build_release,"risc64")
release-static:
	$(call build_release,"static")
release-tarball:
	$(call build_release,"tarball")

.PHONY: tags
tags:
	rm -fv TAGS
	find . -name "*.c" -print | etags --append -
	find . -name "*.h" -print | etags --append -
	find . -name "*.cpp" -print | etags --append -
	find . -name "*.hpp" -print | etags --append -


-include $(DEPS)
