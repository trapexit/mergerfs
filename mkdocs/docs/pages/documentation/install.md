# INSTALL

https://github.com/trapexit/mergerfs/releases

If your distribution's package manager includes mergerfs check if the
version is up to date. If out of date it is recommended to use
the latest release found on the release page. Details for common
distros are below.

#### Debian

Most Debian installs are of a stable branch and therefore do not have
the most up to date software. While mergerfs is available via `apt` it
is suggested that users install the most recent version available from
the [releases page](https://github.com/trapexit/mergerfs/releases).

#### prebuilt deb

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs_<ver>.debian-<rel>_<arch>.deb
dpkg -i mergerfs_<ver>.debian-<rel>_<arch>.deb
```

#### apt

```
sudo apt install -y mergerfs
```

#### Ubuntu

Most Ubuntu installs are of a stable branch and therefore do not have
the most up to date software. While mergerfs is available via `apt` it
is suggested that users install the most recent version available from
the [releases page](https://github.com/trapexit/mergerfs/releases).

#### prebuilt deb

```
wget https://github.com/trapexit/mergerfs/releases/download/<version>/mergerfs_<ver>.ubuntu-<rel>_<arch>.deb
dpkg -i mergerfs_<ver>.ubuntu-<rel>_<arch>.deb
```

#### apt

```
sudo apt install -y mergerfs
```

#### Raspberry Pi OS

Effectively the same as Debian or Ubuntu.

#### Fedora

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.fc<rel>.<arch>.rpm
sudo rpm -i mergerfs-<ver>.fc<rel>.<arch>.rpm
```

#### CentOS / Rocky

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.el<rel>.<arch>.rpm
sudo rpm -i mergerfs-<ver>.el<rel>.<arch>.rpm
```

#### ArchLinux

1. Setup AUR
2. Install `mergerfs`

#### Other

Static binaries are provided for situations where native packages are
unavailable.

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-static-linux_<arch>.tar.gz
sudo tar xvf mergerfs-static-linux_<arch>.tar.gz -C /
```
