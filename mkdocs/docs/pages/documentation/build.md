# BUILD

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

#### Debian / Ubuntu

```
$ cd mergerfs
$ sudo tools/install-build-pkgs
$ make deb
$ sudo dpkg -i ../mergerfs_<version>_<arch>.deb
```

#### RHEL / CentOS / Rocky / Fedora

```
$ su -
```
