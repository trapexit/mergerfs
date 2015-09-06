# The MIT License (MIT)
#
# Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

PKGCONFIG =  	$(shell which pkg-config)
GIT 	  = 	$(shell which git)
TAR 	  = 	$(shell which tar)
MKDIR     = 	$(shell which mkdir)
TOUCH 	  = 	$(shell which touch)
CP        = 	$(shell which cp)
RM 	  = 	$(shell which rm)
LN        =     $(shell which ln)
FIND 	  = 	$(shell which find)
INSTALL   = 	$(shell which install)
MKTEMP    = 	$(shell which mktemp)
STRIP     = 	$(shell which strip)
PANDOC    =	$(shell which pandoc)
SED       =     $(shell which sed)
GZIP      =     $(shell which gzip)
RPMBUILD  =     $(shell which rpmbuild)
GIT2DEBCL =     ./tools/git2debcl
CPPFIND   =     ./tools/cppfind

ifeq ($(PKGCONFIG),"")
$(error "pkg-config not installed")
endif

ifeq ($(PANDOC),"")
$(warning "pandoc does not appear available: manpage won't be buildable")
endif

XATTR_AVAILABLE = $(shell test ! -e /usr/include/attr/xattr.h; echo $$?)

FUSE_AVAILABLE = $(shell ! pkg-config --exists fuse; echo $$?)

ifeq ($(FUSE_AVAILABLE),0)
FUSE_AVAILABLE = $(shell test ! -e /usr/include/fuse.h; echo $$?)
endif

ifeq ($(FUSE_AVAILABLE),0)
$(error "FUSE development package doesn't appear available")
endif

FLAG_NOPATH = $(shell $(CPPFIND) "flag_nopath")
FALLOCATE   = $(shell $(CPPFIND) "fuse_fs_fallocate")
FLOCK       = $(shell $(CPPFIND) "fuse_fs_flock")
READ_BUF    = $(shell $(CPPFIND) "fuse_fs_read_buf")
WRITE_BUF   = $(shell $(CPPFIND) "fuse_fs_write_buf")

OPTS 	    = -O2
SRC	    = $(wildcard src/*.cpp)
OBJ         = $(SRC:src/%.cpp=obj/%.o)
DEPS        = $(OBJ:obj/%.o=obj/%.d)
TARGET      = mergerfs
MANPAGE     = $(TARGET).1
FUSE_CFLAGS = $(shell $(PKGCONFIG) --cflags fuse)
CFLAGS      = -g -Wall \
	      $(OPTS) \
              $(FUSE_CFLAGS) \
              -DFUSE_USE_VERSION=26 \
              -MMD \
	      -DFLAG_NOPATH=$(FLAG_NOPATH) \
	      -DFALLOCATE=$(FALLOCATE) \
              -DFLOCK=$(FLOCK) \
	      -DREAD_BUF=$(READ_BUF) \
              -DWRITE_BUF=$(WRITE_BUF)
LDFLAGS       = $(shell $(PKGCONFIG) fuse --libs)

PREFIX        = /usr/local
EXEC_PREFIX   = $(PREFIX)
DATAROOTDIR   = $(PREFIX)/share
DATADIR       = $(DATAROOTDIR)
BINDIR        = $(EXEC_PREFIX)/bin
MANDIR        = $(DATAROOTDIR)/man
MAN1DIR       = $(MANDIR)/man1

INSTALLBINDIR  = $(DESTDIR)$(BINDIR)
INSTALLMAN1DIR = $(DESTDIR)$(MAN1DIR)

ifeq ($(XATTR_AVAILABLE),0)
$(warning "xattr not available: disabling")
CFLAGS += -DWITHOUT_XATTR
endif

all: $(TARGET) clonepath

help:
	@echo "usage: make"
	@echo "make XATTR_AVAILABLE=0 - to build program without xattrs functionality (auto discovered otherwise)"

$(TARGET): src/version.hpp obj/obj-stamp $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

clonepath: $(TARGET)
	$(LN) -s $< $@

changelog:
	$(GIT2DEBCL) --name $(TARGET) > ChangeLog

authors:
	$(GIT) log --format='%aN <%aE>' | sort -f | uniq > AUTHORS

src/version.hpp:
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	@echo "#ifndef _VERSION_HPP" > src/version.hpp
	@echo "#define _VERSION_HPP" >> src/version.hpp
	@echo "static const char MERGERFS_VERSION[] = \"$(VERSION)\";" >> src/version.hpp
	@echo "#endif" >> src/version.hpp

obj/obj-stamp:
	$(MKDIR) -p obj
	$(TOUCH) $@

obj/%.o: src/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean: rpm-clean
	$(RM) -rf obj
	$(RM) -f src/version.hpp
	$(RM) -f "$(TARGET)" "$(MANPAGE)" clonepath
	$(FIND) . -name "*~" -delete

distclean: clean
	$(GIT) clean -fd

install: install-base install-clonepath install-man

install-base: $(TARGET)
	$(INSTALL) -v -m 0755 -D "$(TARGET)" "$(INSTALLBINDIR)/$(TARGET)"

install-clonepath: clonepath
	$(CP) -a $< "$(INSTALLBINDIR)/$<"

install-man: $(MANPAGE)
	$(INSTALL) -v -m 0644 -D "$(MANPAGE)" "$(INSTALLMAN1DIR)/$(MANPAGE)"

install-strip: install-base
	$(STRIP) "$(INSTALLBINDIR)/$(TARGET)"

uninstall: uninstall-base uninstall-clonepath uninstall-man

uninstall-base:
	$(RM) -f "$(INSTALLBINDIR)/$(TARGET)"

uninstall-clonepath:
	$(RM) -f "$(INSTALLBINDIR)/clonepath"

uninstall-man:
	$(RM) -f "$(INSTALLMAN1DIR)/$(MANPAGE)"

$(MANPAGE): README.md
	$(PANDOC) -s -t man -o $(MANPAGE) README.md

man: $(MANPAGE)

tarball: clean man changelog authors src/version.hpp
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(eval FILENAME := $(TARGET)-$(VERSION))
	$(eval TMPDIR := $(shell $(MKTEMP) --tmpdir -d .$(FILENAME).XXXXXXXX))
	$(MKDIR) $(TMPDIR)/$(FILENAME)
	$(CP) -ar . $(TMPDIR)/$(FILENAME)
	$(TAR) --exclude=.git -cz -C $(TMPDIR) -f $(FILENAME).tar.gz $(FILENAME)
	$(RM) -rf $(TMPDIR)

debian-changelog:
	$(GIT2DEBCL) --name $(TARGET) > debian/changelog

deb: debian-changelog
	dpkg-buildpackage

unsigned-deb: debian-changelog
	dpkg-buildpackage -uc -us

rpm-clean:
	$(RM) -rf rpmbuild

rpm: tarball
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	$(eval VERSION := $(subst -,_,$(VERSION)))
	$(MKDIR) -p rpmbuild/{BUILD,RPMS,SOURCES}
	$(SED) 's/__VERSION__/$(VERSION)/g' $(TARGET).spec > \
		rpmbuild/SOURCES/$(TARGET).spec
	cp -ar $(TARGET)-$(VERSION).tar.gz rpmbuild/SOURCES
	$(RPMBUILD) -ba rpmbuild/SOURCES/$(TARGET).spec \
		--define "_topdir $(CURDIR)/rpmbuild"

.PHONY: all clean install help

include $(wildcard obj/*.d)
