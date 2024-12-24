# Installation

If you are using a non-rolling release Linux distro such as Debian or
Ubuntu then you are almost certainly going to have an old version of
mergerfs installed if you use the "official" package. For that reason
we provide packages for major stable released distros.

Before reporting issues or bugs please be sure to upgrade to the
latest release to confirm they still exist.

All provided packages can be found at [https://github.com/trapexit/mergerfs/releases](https://github.com/trapexit/mergerfs/releases)


## Debian

Most Debian installs are of a stable branch and therefore do not have
the most up to date software. While mergerfs is available via `apt` it
is suggested that users install the most recent version available from
the [releases page](https://github.com/trapexit/mergerfs/releases).


### prebuilt deb

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs_<ver>.debian-<rel>_<arch>.deb
sudo dpkg -i mergerfs_<ver>.debian-<rel>_<arch>.deb
```


### apt

```
sudo apt install -y mergerfs
```


## Ubuntu

Most Ubuntu installs are of a stable branch and therefore do not have
the most up to date software. While mergerfs is available via `apt` it
is suggested that users install the most recent version available from
the [releases page](https://github.com/trapexit/mergerfs/releases).


### prebuilt deb

```
wget https://github.com/trapexit/mergerfs/releases/download/<version>/mergerfs_<ver>.ubuntu-<rel>_<arch>.deb
sudo dpkg -i mergerfs_<ver>.ubuntu-<rel>_<arch>.deb
```


### apt

```
sudo apt install -y mergerfs
```


## Raspberry Pi OS

The same as Debian or Ubuntu.


## Fedora

Get the RPM from the [releases page](https://github.com/trapexit/mergerfs/releases).

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.fc<rel>.<arch>.rpm
sudo rpm -i mergerfs-<ver>.fc<rel>.<arch>.rpm
```


## CentOS / Rocky

Get the RPM from the [releases page](https://github.com/trapexit/mergerfs/releases).

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.el<rel>.<arch>.rpm
sudo rpm -i mergerfs-<ver>.el<rel>.<arch>.rpm
```


## ArchLinux

1. Setup AUR
2. `pacman -S mergerfs`


## Other

Static binaries are provided for situations where native packages are
unavailable.

Get the tarball from the [releases page](https://github.com/trapexit/mergerfs/releases).

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-static-linux_<arch>.tar.gz
sudo tar xvf mergerfs-static-linux_<arch>.tar.gz -C /
```
