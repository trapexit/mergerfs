# Installation

If you are using a non-rolling release Linux distro such as Debian or
Ubuntu then you are almost certainly going to have an old version of
mergerfs installed if you use the "official" package. For that reason
we provide packages for major stable released distros.

Before reporting issues or bugs please be sure to upgrade to the
latest release to confirm they still exist.

All provided packages can be found at [https://github.com/trapexit/mergerfs/releases](https://github.com/trapexit/mergerfs/releases)

## ArchLinux

1. Setup AUR
2. `pacman -S mergerfs`

## CentOS / Rocky

Get the RPM from the [releases page](https://github.com/trapexit/mergerfs/releases).

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.el<rel>.<arch>.rpm
sudo rpm -i mergerfs-<ver>.el<rel>.<arch>.rpm
```

## Debian based OS

Debian/Ubuntu/Raspberry Pi

Most Debian based OS installs are of a stable branch and therefore do not have
the most up to date software. While mergerfs is available via `apt` it
is suggested that users install the most recent version available from
the [releases page](https://github.com/trapexit/mergerfs/releases).

### prebuilt deb

#### Debian

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs_<ver>.debian-<rel>_<arch>.deb
sudo dpkg -i mergerfs_<ver>.debian-<rel>_<arch>.deb
```

#### Ubuntu

Given non-LTS releases are rarely used for servers and only have [9
months of support](https://ubuntu.com/about/release-cycle) we do not
build packages for them by default. It is recommended that you first
try the `deb` file for the LTS release most closely related to your
non-LTS release. If that does not work you can use the [static
build](#static-linux-binaries) or [file a
ticket](https://github.com/trapexit/mergerfs/issues) requesting a
package built.


```
wget https://github.com/trapexit/mergerfs/releases/download/<version>/mergerfs_<ver>.ubuntu-<rel>_<arch>.deb
sudo dpkg -i mergerfs_<ver>.ubuntu-<rel>_<arch>.deb
```

### apt

```
sudo apt install -y mergerfs
```

## Fedora

Get the RPM from the [releases page](https://github.com/trapexit/mergerfs/releases).

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.fc<rel>.<arch>.rpm
sudo rpm -i mergerfs-<ver>.fc<rel>.<arch>.rpm
```

## SUSE/OpenSUSE

Avaliable at [filesystems repo](https://build.opensuse.org/package/show/filesystems/mergerfs)

if you have not added the repo (e.g. in case of OpenSUSE Tumbleweed, rel=openSUSE_Tumbleweed): 

```
zypper addrepo https://download.opensuse.org/repositories/filesystems/<rel>/filesystems.repo
zypper refresh
zypper install mergerfs
```

## FreeBSD

[https://www.freshports.org/filesystems/mergerfs](https://www.freshports.org/filesystems/mergerfs)

```
pkg install filesystems/mergerfs
```

## NixOS

[search.nixos.org](https://search.nixos.org/packages?channel=unstable&show=mergerfs&from=0&size=50&sort=relevance&type=packages&query=mergerfs)

```
nix-env -iA nixos.mergerfs
```

## Other Linux Distros

[Check your distro.](../related_projects.md#distributions-including-mergerfs)

## Static Linux Binaries

If your distro does not package mergerfs there are static binaries
provided.

Get the tarball from the [releases page](https://github.com/trapexit/mergerfs/releases).

```
wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-static-linux_<arch>.tar.gz
sudo tar xvf mergerfs-static-linux_<arch>.tar.gz -C /
```
