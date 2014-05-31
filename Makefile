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

XATTR_AVAILABLE = $(shell test ! -e /usr/include/attr/xattr.h; echo $$?)
FUSE_AVAILABLE = $(shell ! pkg-config --exists fuse; echo $$?)
ifeq ($(FUSE_AVAILABLE),0)
FUSE_AVAILABLE = $(shell test ! -e /usr/include/fuse.h; echo $$?)
endif

ifeq ($(FUSE_AVAILABLE),0)
$(error "FUSE development package doesn't appear available")
endif

PKGCONFIG =  	$(shell which pkg-config)
GIT 	= 	$(shell which git)
TAR 	= 	$(shell which tar)
MKDIR   = 	$(shell which mkdir)
TOUCH 	= 	$(shell which touch)
CP      = 	$(shell which cp)
RM 	= 	$(shell which rm)
FIND 	= 	$(shell which find)
INSTALL = 	$(shell which install)
MKTEMP  = 	$(shell which mktemp)
STRIP   = 	$(shell which strip)

SRC	=	$(wildcard src/*.cpp)
OBJ     =       $(SRC:src/%.cpp=obj/%.o)
DEPS    =       $(OBJ:obj/%.o=obj/%.d)
TARGET  =       mergerfs
CFLAGS  =       -g -Wall \
                $(shell $(PKGCONFIG) fuse --cflags) \
                -DFUSE_USE_VERSION=26 \
                -MMD
LDFLAGS	=       $(shell $(PKGCONFIG) fuse --libs)

BINDIR 	= 	$(PREFIX)/usr/bin
MANDIR 	= 	$(PREFIX)/share/man/man1
INSTALLPATH = 	$(DESTDIR)/$(BINDIR)
INSTALLTARGET = $(INSTALLPATH)/$(TARGET)

ifeq ($(XATTR_AVAILABLE),0)
CFLAGS += -DWITHOUT_XATTR
endif

all: $(TARGET)

help:
	@echo "usage: make"
	@echo "make XATTR_AVAILABLE=0 - to build program without xattrs functionality (auto discovered otherwise)"

$(TARGET): obj/obj-stamp $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

changelog:
	$(GIT) log --pretty --numstat --summary | git2cl > ChangeLog

obj/obj-stamp:
	$(MKDIR) -p obj
	$(TOUCH) $@

obj/%.o: src/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -rf obj "$(TARGET)"
	$(FIND) -name "*~" -delete

distclean: clean
	$(GIT) clean -ifd

install: $(TARGET)
	$(INSTALL) -m 0755 -D "$(TARGET)" "$(INSTALLTARGET)"

install-strip: install
	$(STRIP) "$(INSTALLTARGET)"

uninstall:
	$(RM) "$(INSTALLTARGET)"

tarball: distclean changelog
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	$(eval FILENAME := $(TARGET)-$(VERSION))
	$(eval TMPDIR := $(shell $(MKTEMP) --tmpdir -d .$(FILENAME).XXXXXXXX))
	$(MKDIR) $(TMPDIR)/$(FILENAME)
	$(CP) -ar . $(TMPDIR)/$(FILENAME)
	$(TAR) --exclude=.git -cz -C $(TMPDIR) -f ../$(FILENAME).tar.gz $(FILENAME)
	$(RM) -rf $(TMPDIR)

deb:
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	$(GIT) dch --auto --release --new-version="$(VERSION)"
	$(GIT) buildpackage --git-ignore-new

.PHONY: all clean install help

include $(wildcard obj/*.d)
