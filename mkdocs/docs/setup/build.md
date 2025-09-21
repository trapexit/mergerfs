# Build

**NOTE:** Prebuilt packages can be found at and recommended for most
users: https://github.com/trapexit/mergerfs/releases

**NOTE:** Only tagged releases are supported. `master` and other
branches should be considered works in progress.

First, get the code from [github](https://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.tar.gz
```

## Debian / Ubuntu

```
$ cd mergerfs
$ sudo buildtools/install-build-pkgs
$ make deb
$ sudo dpkg -i ../mergerfs_<version>_<arch>.deb
```

## RHEL / CentOS / Rocky / Fedora

```
$ su -
# cd mergerfs
# buildtools/install-build-pkgs
# make rpm
# rpm -i rpmbuild/RPMS/<arch>/mergerfs-<version>.<arch>.rpm
```

## Generic Linux

Have git, g++ or clang, make, python installed.


```
$ cd mergerfs
$ make
$ sudo make install
```

## FreeBSD

Have git, g++ or clang, gmake, python installed.

```
$ cd mergerfs
$ gmake
$ gmake install # as root
```


## Build options

```
$ make help
usage: make

make USE_XATTR=0      - build program without xattrs functionality
make STATIC=1         - build static binary
make LTO=1            - build with link time optimization
```
