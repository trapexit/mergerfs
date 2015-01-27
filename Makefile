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
GIT2DEBCL =     ./tools/git2debcl
CPPFIND   =     ./tools/cppfind

ifeq ($(PKGCONFIG),"")
$(error "pkg-config not installed"
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

BINDIR 	      =	$(PREFIX)/usr/bin
MANDIR 	      =	$(PREFIX)/usr/share/man/man1
INSTALLBINDIR = $(DESTDIR)$(BINDIR)
INSTALLMANDIR = $(DESTDIR)$(MANDIR)

ifeq ($(XATTR_AVAILABLE),0)
$(warning "xattr not available: disabling")
CFLAGS += -DWITHOUT_XATTR
endif

KERNEL = $(shell uname -s)
ifeq ($(KERNEL),Linux)
	CFLAGS += -DLINUX
endif
ifeq ($(KERNEL),Darwin)
	CFLAGS += -DOSX
endif

all: $(TARGET) clonepath

help:
	@echo "usage: make"
	@echo "make XATTR_AVAILABLE=0 - to build program without xattrs functionality (auto discovered otherwise)"

$(TARGET): obj/obj-stamp $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

clonepath: $(TARGET)
	$(LN) -s $< $@

changelog:
	$(GIT) log --pretty --numstat --summary | git2cl > ChangeLog

obj/obj-stamp:
	$(MKDIR) -p obj
	$(TOUCH) $@

obj/%.o: src/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -rf obj
	$(RM) -f "$(TARGET)" "$(MANPAGE)" clonepath
	$(FIND) -name "*~" -delete

distclean: clean
	$(GIT) clean -fd

install: install-base install-clonepath install-man

install-base: $(TARGET)
	$(INSTALL) -v -m 0755 -D "$(TARGET)" "$(INSTALLBINDIR)/$(TARGET)"

install-clonepath: clonepath
	$(CP) -a $< "$(INSTALLBINDIR)/$<"

install-man: $(MANPAGE)
	$(INSTALL) -v -m 0644 -D "$(MANPAGE)" "$(INSTALLMANDIR)/$(MANPAGE)"

install-strip: install-base
	$(STRIP) "$(INSTALLBINDIR)/$(TARGET)"

uninstall: uninstall-base uninstall-clonepath uninstall-man

uninstall-base:
	$(RM) -f "$(INSTALLBINDIR)/$(TARGET)"

uninstall-clonepath:
	$(RM) -f "$(INSTALLBINDIR)/clonepath"

uninstall-man:
	$(RM) -f "$(INSTALLMANDIR)/$(MANPAGE")

$(MANPAGE): README.md
	$(PANDOC) -s -t man -o $(MANPAGE) README.md

man: $(MANPAGE)

tarball: clean changelog man
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	$(eval FILENAME := $(TARGET)-$(VERSION))
	$(eval TMPDIR := $(shell $(MKTEMP) --tmpdir -d .$(FILENAME).XXXXXXXX))
	$(MKDIR) $(TMPDIR)/$(FILENAME)
	$(CP) -ar . $(TMPDIR)/$(FILENAME)
	$(TAR) --exclude=.git -cz -C $(TMPDIR) -f ../$(FILENAME).tar.gz $(FILENAME)
	$(RM) -rf $(TMPDIR)

deb:
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	$(GIT2DEBCL) $(TARGET) $(VERSION) > debian/changelog
	$(GIT) buildpackage --git-ignore-new

.PHONY: all clean install help

include $(wildcard obj/*.d)
